admin("box.insert(0, 1, 'te-test')")
admin("box.select(0, 0, 1)")

admin("box.on_request()")
admin("box.on_request(123)")
admin("id = box.on_request(function(...) error('exception') end) ")
admin("box.select(0, 0, 1)")


admin("box.remove_on_request(id)")
admin("box.remove_on_request(id)")
admin("box.select(0, 0, 1)")
admin("id = box.on_request(function(req, skip) skip() end)")
admin("box.select(0, 0, 1)")
admin("box.remove_on_request(id)")


admin("id = box.on_request(function(req, skip) return { 1, 're-test' } end)")
admin("box.select(0, 0, 1)")


admin("box.remove_on_request(id)")
admin("id = box.on_request(function(req, skip) return req.type end)")
admin("box.select(23, 12, 1)")
admin("box.delete(24, 12, 1)")
admin("box.update(25, 12, '=p', 1, 2)")
admin("box.insert(26, 2, 2, 3)")
admin("box.replace(27, 2, 2, 3)")

admin("box.remove_on_request(id)")
admin("id = box.on_request(function(req, skip) return req.space_id end)")
admin("box.select(23, 12, 1)")
admin("box.delete(24, 12, 1)")
admin("box.update(25, 12, '=p', 1, 2)")
admin("box.insert(26, 2, 2, 3)")
admin("box.replace(27, 2, 2, 3)")


admin("box.remove_on_request(id)")
admin("id = box.on_request(function(req, skip) return req.flags end)")
admin("box.select(23, 12, 1)")
admin("box.delete(24, 12, 1)")
admin("box.update(25, 12, '=p', 1, 2)")
admin("box.insert(26, 2, 2, 3)")
admin("box.replace(27, 2, 2, 3)")

admin("box.remove_on_request(id)")
admin("id = box.on_request(function(req, skip) return req.key end)")
admin("box.select(23, 12, 1)")
admin("box.delete(24, 12, 1)")
admin("box.update(25, 12, '=p', 1, 2)")
admin("box.insert(26, 2, 2, 3)")

admin("box.remove_on_request(id)")
admin("id = box.on_request(function(req, skip) return req.tuple end)")
admin("box.replace(27, 2, 2, 3)")

admin("box.remove_on_request(id)")
admin("id = box.on_request(function(req, skip) return unpack(req.keys) end)")
admin("box.select(23, 12, 1)")

admin("box.remove_on_request(id)")
admin("id = box.on_request(function(req, skip) return req.index_id end)")
admin("box.select(23, 12, 1)")

admin("box.remove_on_request(id)")
admin("id = box.on_request(function(req, skip) return req.offset end)")
admin("box.select(23, 12, 1)")

admin("box.remove_on_request(id)")
admin("id = box.on_request(function(req, skip) return req.limit end)")
admin("box.select(23, 12, 1)")

admin("box.remove_on_request(id)")
admin("id = box.on_request(function(req, skip) return req.expr end)")
admin("box.update(23, 12, '=p!p#p', 11, 2, 12, 3, 34, 4)")

admin("box.remove_on_request(id)")
admin("id = box.on_request(function(req, skip) return req.expr end)")
admin("box.update(23, 12, '=p!p#p&p:p', 11, 2, 12, 3, 34, 4, 22, 21, 27, box.pack('ppp', 223, 3332, 'abc'))")
