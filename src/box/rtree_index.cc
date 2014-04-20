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
#include "rtree_index.h"
#include "tuple.h"
#include "space.h"
#include "exception.h"
#include "errinj.h"

/* {{{ Utilities. *************************************************/

inline coord_t extract_coord(struct tuple const* tuple, int field_no) 
{
        const char* field = tuple_field(tuple, field_no);
        return (coord_t)mp_decode_uint(&field);
}

inline void extract_rectangle(rectangle_t& r, struct tuple const* tuple, struct key_def* kd) 
{
        switch (kd->part_count) { 
        case 2: // point
                r.boundary[0] = r.boundary[2] = extract_coord(tuple, kd->parts[0].fieldno);
                r.boundary[1] = r.boundary[3] = extract_coord(tuple, kd->parts[1].fieldno);
                break;
        case 4: // rectangle
                for (int i = 0; i < 4; i++) { 
                        r.boundary[i] = extract_coord(tuple, kd->parts[i].fieldno);
                }
                break;
        default:
                assert(false);
        }
}
/* {{{ TreeIndex Iterators ****************************************/

struct rtree_iterator {
        struct iterator base;
        R_tree_iterator impl;
};

static void
rtree_iterator_free(struct iterator *i)
{
        delete (rtree_iterator*)i;
}

static struct tuple *
rtree_iterator_next(struct iterator *i)
{
        return (tuple*)((rtree_iterator*)i)->impl.next();
}

/* }}} */

/* {{{ TreeIndex  **********************************************************/

RTreeIndex::RTreeIndex(struct key_def *key_def)
	: Index(key_def)
{
        if (key_def->part_count != 2 && key_def->part_count != 4) { 
                tnt_raise(ClientError, ER_UNSUPPORTED,
                          "R-Tree index can be defied only for points (two parts) or rectangles (four parts)");
        }
        for (int i = 0; i < key_def->part_count; i++) { 
                if (key_def->parts[i].type != NUM) { 
                        tnt_raise(ClientError, ER_UNSUPPORTED,
                                  "R-Tree index can be defied only for numeric fields");
                }
        }    
}

size_t
RTreeIndex::size() const
{
	return tree.size();
}

size_t
RTreeIndex::memsize() const
{
        return tree.size() * (sizeof(rectangle_t) + sizeof(struct tuple *)) * 2; // multiply on two because up to half of tree pages are not used 
}

struct tuple *
RTreeIndex::findByKey(const char *key, uint32_t part_count) const
{
        rectangle_t r;
        R_tree_iterator iterator;
        switch (part_count) { 
        case 2:
                r.boundary[0] = r.boundary[2] = mp_decode_uint(&key);
                r.boundary[1] = r.boundary[3] = mp_decode_uint(&key);
                break;
        case 4:
                for (int i = 0; i < 4; i++) { 
                        r.boundary[i] = mp_decode_uint(&key);
                }
                break;
        default:
                tnt_raise(ClientError, ER_UNSUPPORTED,
                          "R-Tree key should be point (two integer coordinates) or rectangles (four integer coordinates)");
        }
        if (tree.search(r, SOP_OVERLAPS, iterator)) {
                return (struct tuple*)iterator.next();
        }
        return NULL;
}    

struct tuple *
RTreeIndex::replace(struct tuple *old_tuple, struct tuple *new_tuple,
                    enum dup_replace_mode)
{
        rectangle_t r;
        if (new_tuple) {
                extract_rectangle(r, new_tuple, key_def);
                tree.insert(r, new_tuple);
        }
	if (old_tuple) {
                extract_rectangle(r, old_tuple, key_def);
                if (!tree.remove(r, old_tuple)) { 
                        old_tuple = NULL;
                }
        }
        return old_tuple;
}

struct iterator *
RTreeIndex::allocIterator() const
{
	rtree_iterator *it = (rtree_iterator *)new rtree_iterator();
	if (it == NULL) {
		tnt_raise(ClientError, ER_MEMORY_ISSUE,
			  sizeof(struct rtree_iterator),
			  "RTreeIndex", "iterator");
	}
	it->base.next = rtree_iterator_next;
	it->base.free = rtree_iterator_free;
	return &it->base;
}

void
RTreeIndex::initIterator(struct iterator *iterator, enum iterator_type type,
                         const char *key, uint32_t part_count) const
{
        rectangle_t r;
        rtree_iterator *it = (rtree_iterator *)iterator;
        switch (part_count) { 
        case 0:
                if (type != ITER_ALL) { 
                        tnt_raise(ClientError, ER_UNSUPPORTED,
				  "It is possible to omit key only for ITER_ALL");
                }
                break;
        case 2:
                r.boundary[0] = r.boundary[2] = mp_decode_uint(&key);
                r.boundary[1] = r.boundary[3] = mp_decode_uint(&key);
                break;
        case 4:
                for (int i = 0; i < 4; i++) { 
                        r.boundary[i] = mp_decode_uint(&key);
                }
                break;
        default:
                tnt_raise(ClientError, ER_UNSUPPORTED,
                          "R-Tree key should be point (two integer coordinates) or rectangles (four integer coordinates)");
        }
        Spatial_search_op op;
        switch (type) { 
        case ITER_ALL:
                op = SOP_ALL;
                break;
        case ITER_EQ:
                op = SOP_EQUALS;
                break;
        case ITER_GT:
                op = SOP_STRICT_CONTAINS;
                break;
        case ITER_GE:
                op = SOP_CONTAINS;
                break;
        case ITER_LT:
                op = SOP_STRICT_BELONGS;
                break;
        case ITER_LE:
                op = SOP_BELONGS;
                break;
        case ITER_OVERLAPS:
                op = SOP_OVERLAPS;
                break;
        case ITER_NEIGHBOR:
                op = SOP_NEIGHBOR;
                break;
        default:
                tnt_raise(ClientError, ER_UNSUPPORTED,
                          "Unsupported search operation %d for R-Tree", type);
        }
        tree.search(r, op, it->impl);
}

void
RTreeIndex::beginBuild()
{
    tree.purge();
}


