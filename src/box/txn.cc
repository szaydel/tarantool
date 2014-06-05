/*
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the
 *    following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY <COPYRIGHT HOLDER> ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * <COPYRIGHT HOLDER> OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include "txn.h"
#include "session.h"
#include "box.h"
#include "tuple.h"
#include "space.h"
#include <tarantool.h>
#include "cluster.h"
#include "recovery.h"
#include <fiber.h>
#include "request.h" /* for request_name */

double too_long_threshold;
int multistatement_transaction_limit;

static txn_request* new_txn_request(struct txn *txn)
{
        txn_request* tr;
        txn->n_requests += 1;
        if (multistatement_transaction_limit != 0 && txn->n_requests > multistatement_transaction_limit) {
                txn_current() = NULL;
		tnt_raise(LoggedError, ER_TRANSACTION_TOO_LONG, multistatement_transaction_limit);
        }
        if (txn->tail == NULL) {
                txn->tail = tr = &txn->req;
        } else {
                tr = (txn_request*)region_alloc0(&fiber()->gc, sizeof(*tr));
                txn->tail = txn->tail->next = tr;
        }
        tr->next = NULL;
        return tr;
}


void
txn_add_redo(struct txn *txn, struct request *request)
{
        txn_request* tr = new_txn_request(txn);
	tr->row = request->header;
	if (recovery_state->wal_mode == WAL_NONE || request->header != NULL) {
		return;
        }
        struct iproto_header *row = (struct iproto_header *)
		region_alloc0(&fiber()->gc, sizeof(struct iproto_header));
	row->type = request->type;
	row->bodycnt = request_encode(request, row->body);
	tr->row = row;
}

void
txn_replace(struct txn *txn, struct space *space,
	    struct tuple *old_tuple, struct tuple *new_tuple,
	    enum dup_replace_mode mode)
{
	assert(old_tuple || new_tuple);
        txn_request* tr = txn->tail;
        assert(tr != NULL);
	/*
	 * Remember the old tuple only if we replaced it
	 * successfully, to not remove a tuple inserted by
	 * another transaction in rollback().
	 */
	tr->old_tuple = space_replace(space, old_tuple, new_tuple, mode);
	if (new_tuple) {
		tr->new_tuple = new_tuple;
		tuple_ref(tr->new_tuple, 1);
	}
	tr->space = space;
	/*
	 * Run on_replace triggers. For now, disallow mutation
	 * of tuples in the trigger.
	 */
	if (! rlist_empty(&space->on_replace) && space->run_triggers)
		trigger_run(&space->on_replace, txn);
}

struct txn *
txn_begin()
{
	struct txn *txn = (struct txn *)
		region_alloc0(&fiber()->gc, sizeof(*txn));
	rlist_create(&txn->on_commit);
	rlist_create(&txn->on_rollback);
        txn->tail = NULL;
        txn->nesting_level = 1;
        txn->n_requests = 0;
        txn->outer = txn_current();
	return txn;
}

void
txn_commit(struct txn *txn)
{
        if (txn->tail == NULL) {
                return; /* nothing to commit */
        }
        txn_request* tr = &txn->req;
        int flags = WAL_REQ_FLAG_IS_FIRST|WAL_REQ_FLAG_IN_TRANS|WAL_REQ_FLAG_HAS_NEXT;

        // First set flags
        struct iproto_header* last_row = NULL;
        do {
                if ((tr->old_tuple || tr->new_tuple) &&
                    !space_is_temporary(tr->space))
                {
			if (recovery_state->wal_mode != WAL_NONE) {
				struct iproto_header* row = tr->row;
				assert(row != NULL);
				row->flags = flags;
				last_row = row;
				flags &= ~WAL_REQ_FLAG_IS_FIRST;
			}
		}
        } while ((tr = tr->next) != NULL);

        if (last_row != NULL) {
                if (last_row->flags & WAL_REQ_FLAG_IS_FIRST) { // transaction with single statement
                        last_row->flags &= ~WAL_REQ_FLAG_IN_TRANS;
                }
                last_row->flags &= ~WAL_REQ_FLAG_HAS_NEXT;
	}
	// And now send rows to WAL writers
	tr = &txn->req;
	do {
		if ((tr->old_tuple || tr->new_tuple) &&
		    !space_is_temporary(tr->space))
		{
			struct iproto_header* row = tr->row;
			int res = 0;
			/* txn_commit() must be done after txn_add_redo() */
			assert(recovery_state->wal_mode == WAL_NONE ||
			       row != NULL);
			ev_tstamp start = ev_now(loop()), stop;
			res = wal_write(recovery_state, row);
			stop = ev_now(loop());

			if (stop - start > too_long_threshold && row != NULL) {
				say_warn("too long %s: %.3f sec",
					 iproto_request_name(row->type),
					 stop - start);
			}

			if (res)
				tnt_raise(LoggedError, ER_WAL_IO);
		}
	} while ((tr = tr->next) != NULL);

        trigger_run(&txn->on_commit, txn); /* must not throw. */
}

/**
 * txn_finish() follows txn_commit() on success.
 * It's moved to a separate call to be able to send
 * old tuple to the user before it's deleted.
 */

void
txn_finish(struct txn *txn)
{
        if (txn->tail != NULL) {
                txn_request* tr = &txn->req;
                do {
                        if (tr->old_tuple) {
                                tuple_ref(tr->old_tuple, -1);
                        }
			if (tr->space)
				tr->space->engine->factory->txnFinish(txn);
                } while ((tr = tr->next) != NULL);
        }
        fiber()->on_reschedule_callback = NULL;
        if (txn->outer == NULL) {
                TRASH(txn);
                fiber_gc();
        } else {
                TRASH(txn);
        }
}


void
txn_rollback(struct txn *txn)
{
        if (txn->tail != NULL) {
                txn_request* tr = &txn->req;
                do {
                        if (tr->old_tuple || tr->new_tuple) {
                                space_replace(tr->space, tr->new_tuple,
                                              tr->old_tuple, DUP_INSERT);
                                trigger_run(&txn->on_rollback, tr); /* must not throw. */
                                if (tr->new_tuple) {
                                        tuple_ref(tr->new_tuple, -1);
                                }
                        }
                } while ((tr = tr->next) != NULL);
        }
        fiber()->on_reschedule_callback = NULL;
        if (txn->outer == NULL) {
                TRASH(txn);
                fiber_gc();
        } else {
                TRASH(txn);
        }
}
