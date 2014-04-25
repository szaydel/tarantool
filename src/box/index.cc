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
#include "index.h"
#include "hash_index.h"
#include "tree_index.h"
#include "bitset_index.h"
#include "sophia_index.h"
#include "tuple.h"
#include "say.h"
#include "exception.h"

STRS(iterator_type, ITERATOR_TYPE);

/* {{{ Utilities. **********************************************/

void
key_validate_parts(struct key_def *key_def, const char *key,
		   uint32_t part_count)
{
	(void) key_def;
	(void) key;

	for (uint32_t part = 0; part < part_count; part++) {
		enum mp_type mp_type = mp_typeof(*key);
		mp_next(&key);

		key_mp_type_validate(key_def->parts[part].type, mp_type,
				     ER_KEY_FIELD_TYPE, part);
	}
}

void
key_validate(struct key_def *key_def, enum iterator_type type, const char *key,
	     uint32_t part_count)
{
	assert(key != NULL || part_count == 0);
	if (part_count == 0) {
		/*
		 * Zero key parts are allowed:
		 * - for TREE index, all iterator types,
		 * - ITERA_ALL iterator type, all index types
		 * - ITER_GE iterator in HASH index (legacy)
		 */
		if (key_def->type == TREE || type == ITER_ALL)
			return;
		/* Fall through. */
	}

        if (key_def->type == RTREE) {
                if (part_count != 2 && part_count != 4) {
                        tnt_raise(ClientError, ER_KEY_PART_COUNT,
                                  "R-Tree key should be point (two integer coordinates) or rectangles (four integer coordinates)");
                }
                for (uint32_t part = 0; part < part_count; part++) {
                        enum mp_type mp_type = mp_typeof(*key);
                        mp_next(&key);
                        key_mp_type_validate(NUM, mp_type, ER_KEY_FIELD_TYPE, part);
                }
        } else {
                if (part_count > key_def->part_count)
                        tnt_raise(ClientError, ER_KEY_PART_COUNT,
                                  key_def->part_count, part_count);

                /* Partial keys are allowed only for TREE index type. */
                if (key_def->type != TREE && part_count < key_def->part_count) {
                        tnt_raise(ClientError, ER_EXACT_MATCH,
                                  key_def->part_count, part_count);
                }
                key_validate_parts(key_def, key, part_count);
        }
}

void
primary_key_validate(struct key_def *key_def, const char *key,
		     uint32_t part_count)
{
	assert(key != NULL || part_count == 0);
	if (key_def->part_count != part_count) {
		tnt_raise(ClientError, ER_EXACT_MATCH,
			  key_def->part_count, part_count);
	}
	key_validate_parts(key_def, key, part_count);
}

/* }}} */

/* {{{ Index -- base class for all indexes. ********************/

Index::Index(struct key_def *key_def_arg)
	:key_def(key_def_dup(key_def_arg)),
	m_position(NULL)
{}

void
Index::beginBuild()
{}

void
Index::reserve(uint32_t /* size_hint */)
{}

void
Index::buildNext(struct tuple *tuple)
{
	replace(NULL, tuple, DUP_INSERT);
}

void
Index::endBuild()
{}

Index::~Index()
{
	if (m_position != NULL)
		m_position->free(m_position);
	key_def_delete(key_def);
}

struct tuple *
Index::random(uint32_t rnd) const
{
	(void) rnd;
	tnt_raise(ClientError, ER_UNSUPPORTED,
		  index_type_strs[key_def->type],
		  "random()");
	return NULL;
}

struct tuple *
Index::findByTuple(struct tuple *tuple) const
{
	(void) tuple;
	tnt_raise(ClientError, ER_UNSUPPORTED,
		  index_type_strs[key_def->type],
		  "findByTuple()");
	return NULL;
}

void
index_build(Index *index, Index *pk)
{
	uint32_t n_tuples = pk->size();
	uint32_t estimated_tuples = n_tuples * 1.2;

	index->beginBuild();
	index->reserve(estimated_tuples);

	if (n_tuples > 0) {
		say_info("Adding %" PRIu32 " keys to %s index %"
			 PRIu32 "...", n_tuples,
			 index_type_strs[index->key_def->type], index_id(index));
	}

	struct iterator *it = pk->position();
	pk->initIterator(it, ITER_ALL, NULL, 0);

	struct tuple *tuple;
	while ((tuple = it->next(it)))
		index->buildNext(tuple);

	index->endBuild();
}

/* }}} */
