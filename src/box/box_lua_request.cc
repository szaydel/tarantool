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
#include "box_lua_request.h"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
} /* extern "C" */

#include "request.h"
#include <pickle.h>
#include <say.h>

static int
lbox_request_push_key(struct lua_State *L, const char **keypos,
		      const char *keyend, uint32_t key_part_count)
{
	lua_newtable(L);
	for (uint32_t p = 0; p < key_part_count; p++) {
		uint32_t part_len;
		const char *part = pick_field_str(keypos, keyend, &part_len);
		lua_pushnumber(L, p + 1);
		lua_pushlstring(L, part, part_len);
		lua_settable(L, -3);
	}
	return 1;
}

static int
lbox_request_index(struct lua_State *L)
{
	lua_pushstring(L, "request");
	lua_gettable(L, 1);
	struct request *request = (struct request *) lua_touserdata(L, -1);
	lua_pop(L, 1);

	const char *key = lua_tostring(L, 2);
	if (strcmp(key, "key") == 0) {
		const char *keypos;
		uint32_t key_part_count;
		switch(request->type) {
		case SELECT:
			if (request->s.key_count == 0)
				break;

			/* Extract the first key */
			keypos = request->s.keys;
			key_part_count = pick_u32(&keypos, request->s.keys_end);
			lua_pushvalue(L, 2);
			lbox_request_push_key(L, &keypos, request->s.keys_end,
					      key_part_count);
			lua_rawset(L, 1);
			break;
		case UPDATE:
			keypos = request->u.key;
			lua_pushvalue(L, 2);
			lbox_request_push_key(L, &keypos, request->u.key_end,
					      request->u.key_part_count);
			lua_rawset(L, 1);
			break;
		case DELETE:
		case DELETE_1_3:
			keypos = request->d.key;
			lua_pushvalue(L, 2);
			lbox_request_push_key(L, &request->d.key,
				request->d.key_end, request->d.key_part_count);
			lua_rawset(L, 1);
			break;
		}
	} else if (strcmp(key, "tuple") == 0 && request->type == REPLACE) {
		const char *keypos = request->r.tuple;
		uint32_t key_part_count = pick_u32(&keypos,
						   request->r.tuple_end);
		lua_pushvalue(L, 2);
		lbox_request_push_key(L, &keypos, request->r.tuple_end,
				      key_part_count);
		lua_rawset(L, 1);
	} else if (request->type == SELECT && strcmp(key, "keys") == 0) {
		lua_pushvalue(L, 2);
		lua_newtable(L);
		const char *keypos = request->s.keys;
		for (uint32_t k = 0; k < request->s.key_count; k++) {
			lua_pushnumber(L, k + 1);
			uint32_t key_part_count = pick_u32(&keypos,
				request->s.keys_end);
			lbox_request_push_key(L, &keypos,
				request->s.keys_end, key_part_count);
			lua_settable(L, -3);
		}
		lua_rawset(L, 1);
	} else if (request->type == UPDATE && strcmp(key, "expr") == 0) {
		lua_pushvalue(L, 2);
		lua_pushlstring(L, request->u.expr,
				request->u.expr_end - request->u.expr);
		lua_rawset(L, 1);
	}

	lua_pushvalue(L, 2);
	lua_rawget(L, 1);
	return 1;
}

int
lbox_pushrequest(struct lua_State *L, struct request *request)
{
	lua_newtable(L);
	lua_pushstring(L, "request");
	lua_pushlightuserdata(L, request);
	lua_settable(L, -3);

	lua_newtable(L);
	lua_pushstring(L, "__index");
	lua_pushcclosure(L, lbox_request_index, 0);
	lua_settable(L, -3);
	lua_setmetatable(L, -2);

	lua_pushstring(L, "type_id");
	lua_pushinteger(L, request->type);
	lua_settable(L, -3);

	lua_pushstring(L, "flags");
	lua_pushinteger(L, request->flags);
	lua_settable(L, -3);

	switch(request->type) {
	case SELECT:
		lua_pushstring(L, "type");
		lua_pushstring(L, "SELECT");
		lua_settable(L, -3);
		lua_pushstring(L, "space_id");
		lua_pushinteger(L, request->s.space_no);
		lua_settable(L, -3);
		lua_pushstring(L, "index_id");
		lua_pushinteger(L, request->s.index_no);
		lua_settable(L, -3);
		lua_pushstring(L, "offset");
		lua_pushinteger(L, request->s.offset);
		lua_settable(L, -3);
		lua_pushstring(L, "limit");
		lua_pushinteger(L, request->s.limit);
		lua_settable(L, -3);
		break;
	case REPLACE:
		lua_pushstring(L, "type");
		lua_pushstring(L, "REPLACE");
		lua_settable(L, -3);
		lua_pushstring(L, "space_id");
		lua_pushinteger(L, request->r.space_no);
		lua_settable(L, -3);
		break;
	case UPDATE:
		lua_pushstring(L, "type");
		lua_pushstring(L, "UPDATE");
		lua_settable(L, -3);
		lua_pushstring(L, "space_id");
		lua_pushinteger(L, request->u.space_no);
		lua_settable(L, -3);
		break;
	case DELETE:
	case DELETE_1_3:
		lua_pushstring(L, "type");
		lua_pushstring(L, "DELETE");
		lua_settable(L, -3);
		lua_pushstring(L, "space_id");
		lua_pushinteger(L, request->d.space_no);
		lua_settable(L, -3);
		break;
	case CALL:
		lua_pushstring(L, "type");
		lua_pushstring(L, "CALL");
		lua_settable(L, -3);
		lua_pushstring(L, "procname");
		lua_pushlstring(L, request->c.procname,
				request->c.procname_len);
		lua_settable(L, -3);
		break;
	default:
		assert(false);
	}

	return 1;
}

struct request *
lbox_checkrequest(struct lua_State *L, int narg)
{
	if (!lua_istable(L, narg)) {
		luaL_error(L, "request expected as %d argument", narg);
		return NULL;
	}

	lua_pushstring(L, "request");
	lua_rawget (L, narg);
	struct request *request = (struct request *) lua_touserdata(L, -1);
	if (request == NULL) {
		luaL_error(L, "request expected as %d argument", narg);
		return NULL;
	}
	return request;
}
