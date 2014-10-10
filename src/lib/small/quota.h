#ifndef INCLUDES_TARANTOOL_SMALL_QUOTA_H
#define INCLUDES_TARANTOOL_SMALL_QUOTA_H
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
#if defined(__cplusplus)
extern "C" {
#endif /* defined(__cplusplus) */

/** A basic limit on memory usage */
struct quota {
	size_t used;
	size_t total;
};

static inline void
quota_init(struct quota *quota, size_t total)
{
	quota->used = 0;
	quota->total = total;
}

static inline int
quota_set(struct quota *quota, size_t new_total)
{
	if (new_total > quota->used) {
		quota->total = new_total;
		return  0;
	}
	return -1;
}

/**
 * Provide wrappers around gcc built-ins for now.
 * These built-ins work with all numeric types - may not
 * be the case when another implementation is used.
 */
#define atomic_cas(a, b, c) __sync_val_compare_and_swap(a, b, c)

/** Use up a quota */
static inline int
quota_use(struct quota *quota, size_t size)
{
	for (;;) {
		size_t old_used = quota->used;
		size_t new_used = old_used + size;
		assert(new_used > old_used);
		if (new_used <= quota->total) {
			if (atomic_cas(&quota->used, old_used, new_used) == old_used)
				return 0;
		} else {
			return -1;
		}
	}
}

/** Release used memory */
static inline void
quota_release(struct quota *quota, size_t size)
{
	for (;;) {
		size_t old_used = quota->used;
		assert(size >= old_used);
		size_t new_used = old_used - size;
		if (atomic_cas(&quota->used, old_used, new_used) == old_used)
			return;
	}
}

#undef atomic_cas

#if defined(__cplusplus)
} /* extern "C" { */
#endif /* defined(__cplusplus) */
#endif /* INCLUDES_TARANTOOL_SMALL_QUOTA_H */
