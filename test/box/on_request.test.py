admin("box.insert(0, 1, 'te-test')")
admin("box.select(0, 0, 1)")

admin("box.set_on_request()")
admin("box.set_on_request(123)")
admin("ctx = box.set_on_request(function(...) error('exception') end) ")
admin("box.select(0, 0, 1)")
# true
admin("box.clear_on_request(ctx)")
# false
admin("box.clear_on_request(ctx)")

# Test next() arguments
admin("box.select(0, 0, 1)")
admin("ctx = box.set_on_request(function(ctx, req, next) return next() end)")
admin("box.select(0, 0, 1)")
admin("box.clear_on_request(ctx)")

# Test next with the same request
admin("ctx = box.set_on_request(function(ctx, req, next) return next(req) end)")
admin("box.select(0, 0, 1)")
sql("SELECT * FROM t0 WHERE k0 = 1")
admin("box.replace(0, 1, 2, 3)")
sql("REPLACE INTO t0 VALUES(1, 2, 3)")
admin("box.clear_on_request(ctx)")

# Test multi-return from Lua and BOX_RETURN_TUPLE flag
admin("ctx = box.set_on_request(function(ctx, req, next) t = next(req); return 1,2,3,'Hello' end)")
admin("box.select(0, 0, 1)")
sql("SELECT * FROM t0 WHERE k0 = 1")
admin("box.replace(0, 1, 2, 3)")
sql("REPLACE INTO t0 VALUES(1, 2, 3)")
admin("box.clear_on_request(ctx)")

#
# Test a wrapper that invokes next() and rewrites the result
#
admin("box.select(0, 0, 5)")
admin("ctx = box.set_on_request(function(ctx, req, next) next(req); return 666 end)")
admin("box.select(0, 0, 5)")
sql("SELECT * FROM t0 WHERE k0 = 5")
admin("box.replace(0, 5)")
sql("REPLACE INTO t0 VALUES(5)")
admin("box.select(0, 0, 5)")
sql("SELECT * FROM t0 WHERE k0 = 5")
admin("box.clear_on_request(ctx)")
admin("box.select(0, 0, 5)")
sql("SELECT * FROM t0 WHERE k0 = 5")

#
# Test trigger order
#
admin("ctx = box.set_on_request(function(ctx, req, next)"
        "local tuple = next(req); return {'t[1] value is ', tuple[1]}"
    "end)")
admin("box.select(0, 0, 1)")

admin("ctx2 = box.set_on_request(function(ctx, req, next)"\
        "if req.type == 'CALL' and req.procname == 'ping' then"\
        "    local res = req.args;"\
        "    table.insert(res, 1, 'pong!')"\
        "    return res;"\
        "else"\
        "    return next(req);"\
        "end;"\
    "end)")
sql("CALL ping('12345', '456', '7', '8', '9')")
admin("box.select(0, 0, 1)")
admin("box.clear_on_request(ctx)")
sql("CALL ping()")
admin("box.select(0, 0, 1)")
admin("box.clear_on_request(ctx2)")
sql("CALL ping()")
admin("box.select(0, 0, 1)")

#
# Test context
#

admin("ctx = box.set_on_request(function(ctx2, req, next) assert (ctx2 == ctx) end)")
admin("box.select(0, 0, 1)")
admin("box.clear_on_request(ctx)")

admin("ctx = box.set_on_request(function(ctx, req, next)"\
        "ctx.count = ctx.count + 1;"\
        "return ctx.msg;"\
    "end)")
admin("ctx.msg = 'Check context'"),
admin("ctx.count = 0")
admin("box.select(0, 0, 1)")
admin("ctx.count")
admin("box.clear_on_request(ctx)")

#
# Test request parameters
#

admin("ctx = box.set_on_request(function(ctx, req, next) return req.type end)")
admin("box.select(23, 12, 1)")
admin("box.delete(24, 12, 1)")
admin("box.update(25, 12, '=p', 1, 2)")
admin("box.insert(26, 2, 2, 3)")
admin("box.replace(27, 2, 2, 3)")

admin("box.clear_on_request(ctx)")
admin("ctx = box.set_on_request(function(ctx, req, next) return req.space_id end)")
admin("box.select(23, 12, 1)")
admin("box.delete(24, 12, 1)")
admin("box.update(25, 12, '=p', 1, 2)")
admin("box.insert(26, 2, 2, 3)")
admin("box.replace(27, 2, 2, 3)")


admin("box.clear_on_request(ctx)")
admin("ctx = box.set_on_request(function(ctx, req, next) return req.flags end)")
admin("box.select(23, 12, 1)")
admin("box.delete(24, 12, 1)")
admin("box.update(25, 12, '=p', 1, 2)")
admin("box.insert(26, 2, 2, 3)")
admin("box.replace(27, 2, 2, 3)")

admin("box.clear_on_request(ctx)")
admin("ctx = box.set_on_request(function(ctx, req, next) return req.key end)")
admin("box.select(23, 12, 1)")
admin("box.delete(24, 12, 1)")
admin("box.update(25, 12, '=p', 1, 2)")
admin("box.insert(26, 2, 2, 3)")

admin("box.clear_on_request(ctx)")
admin("ctx = box.set_on_request(function(ctx, req, next) return req.tuple end)")
admin("box.replace(27, 2, 2, 3)")

admin("box.clear_on_request(ctx)")
admin("ctx = box.set_on_request(function(ctx, req, next) return unpack(req.keys) end)")
admin("box.select(23, 12, 1)")

admin("box.clear_on_request(ctx)")
admin("ctx = box.set_on_request(function(ctx, req, next) return req.index_id end)")
admin("box.select(23, 12, 1)")

admin("box.clear_on_request(ctx)")
admin("ctx = box.set_on_request(function(ctx, req, next) return req.offset end)")
admin("box.select(23, 12, 1)")

admin("box.clear_on_request(ctx)")
admin("ctx = box.set_on_request(function(ctx, req, next) return req.limit end)")
admin("box.select(23, 12, 1)")

admin("box.clear_on_request(ctx)")
admin("ctx = box.set_on_request(function(ctx, req, next) return req.expr end)")
admin("box.update(23, 12, '=p!p#p', 11, 2, 12, 3, 34, 4)")

admin("box.clear_on_request(ctx)")
admin("ctx = box.set_on_request(function(ctx, req, next) return req.expr end)")
admin("box.update(23, 12, '=p!p#p&p:p', 11, 2, 12, 3, 34, 4, 22, 21, 27, box.pack('ppp', 223, 3332, 'abc'))")

admin("box.clear_on_request(ctx)")
admin("ctx = box.set_on_request(function(ctx, req, next) tuple = next(req); print(tuple); return tuple end)")
admin("box.replace(0, 1, 2, 3, 4)")
admin("box.clear_on_request(ctx)")
