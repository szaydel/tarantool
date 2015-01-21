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
#include "engine.h"
#include "space.h"
#include "exception.h"
#include "schema.h"
#include "salad/rlist.h"
#include <stdlib.h>
#include <string.h>

RLIST_HEAD(engines);

uint32_t engine_flags[BOX_ENGINE_MAX];
int n_engines;

EngineFactory::EngineFactory(const char *engine_name)
	:name(engine_name),
	 link(RLIST_INITIALIZER(link))
{}

void EngineFactory::init()
{}

void EngineFactory::recover()
{}

void EngineFactory::shutdown()
{}

void EngineFactory::begin(struct txn*, struct space*)
{}

void EngineFactory::commit(struct txn*)
{}

void EngineFactory::rollback(struct txn*)
{}

void EngineFactory::recoveryEvent(enum engine_recovery_event)
{}

Engine::Engine(EngineFactory *f)
	:factory(f)
{
	/* derive recovery state from engine factory */
	initRecovery();
}

/** Register engine factory instance. */
void engine_register(EngineFactory *engine)
{
	rlist_add_tail_entry(&engines, engine, link);
	engine->id = ++n_engines;
	engine_flags[engine->id] = engine->flags;
}

/** Find factory engine by name. */
EngineFactory *
engine_find(const char *name)
{
	EngineFactory *e;
	rlist_foreach_entry(e, &engines, link) {
		if (strcmp(e->name, name) == 0)
			return e;
	}
	tnt_raise(LoggedError, ER_NO_SUCH_ENGINE, name);
}

/** Shutdown all engine factories. */
void engine_shutdown()
{
	EngineFactory *e, *tmp;
	rlist_foreach_entry_safe(e, &engines, link, tmp) {
		e->shutdown();
		delete e;
	}
}

void
engine_begin_recover_snapshot(int64_t snapshot_lsn)
{
	/* recover engine snapshot */
	EngineFactory *f;
	engine_foreach(f) {
		f->snapshot(SNAPSHOT_RECOVER, snapshot_lsn);
	}
}

static void
do_one_recover_step(struct space *space, void * /* param */)
{
	if (space_index(space, 0)) {
		space->engine->recover(space);
	} else {
		/* in case of space has no primary index,
		 * derive it's engine handler recovery state from
		 * the global one. */
		space->engine->initRecovery();
	}
}

void
engine_end_recover_snapshot()
{
	/*
	 * For all new spaces created from now on, when the
	 * PRIMARY key is added, enable it right away.
	 */
	EngineFactory *f;
	engine_foreach(f) {
		f->recoveryEvent(END_RECOVERY_SNAPSHOT);
	}
	space_foreach(do_one_recover_step, NULL);
}

void
engine_end_recover()
{
	/*
	 * For all new spaces created after recovery is complete,
	 * when the primary key is added, enable all keys.
	 */
	EngineFactory *f;
	engine_foreach(f) {
		f->recoveryEvent(END_RECOVERY);
	}
	space_foreach(do_one_recover_step, NULL);
}

