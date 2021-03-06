remote = require('net.box')
env = require('test_run')
test_run = env.new()

--
-- gh-4672: Due to an error in the serializer, -2^63 was
-- serialized as double, although in accordance with the rules of
-- serialization it should be serialized as an integer.
--
format={{name='u', type='unsigned'}, {name='i', type='integer'}}
s = box.schema.space.create('serializer_test_space', {format=format})
_ = s:create_index('ii')

s:insert({1, -2^63})
s:insert({2, -9223372036854775808LL})
s:insert({3, 0})
s:update(3, {{'=', 2, -2^63}})
s:insert({4, 0})
s:update(4, {{'=', 2, -9223372036854775808LL}})

box.schema.user.grant('guest', 'read, write', 'space', 'serializer_test_space')

cn = remote.connect(box.cfg.listen)
s = cn.space.serializer_test_space

s:insert({11, -2^63})
s:insert({12, -9223372036854775808LL})
s:insert({13, 0})
s:update(13, {{'=', 2, -2^63}})
s:insert({14, 0})
s:update(14, {{'=', 2, -9223372036854775808LL}})

cn:close()
box.schema.user.revoke('guest', 'read, write', 'space', 'serializer_test_space')

box.space.serializer_test_space:drop()
