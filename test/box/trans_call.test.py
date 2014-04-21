import os
import sys

admin("box.schema.user.create('test', { password = 'test' })")
admin("box.schema.user.grant('test', 'execute,read,write', 'universe')")
sql.authenticate('test', 'test')

admin("space = box.schema.create_space('tweedledum', { id = 0 })")
admin("space:create_index('primary', { type = 'hash' })")

admin("function myinsert(from,till) space:begin(); for i = from,till,1 do space:insert{i} end; space:commit(); end")
sql("call myinsert(1, 10)")
sql("call myinsert(11, 100)")
sql("call space:len()")

admin("space:drop()")
admin("box.schema.user.drop('test')")

