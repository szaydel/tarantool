space = box.schema.create_space('account')
space:create_index('primary', { type = 'hash' })
for i = 1,100,1 do space:insert({i, 100}) end
--# setopt delimiter ';'
for i = 1,99,1 do 
    space:start_trans()
    space:update({i}, {{'-', 1, 50}})
    space:update({i+1}, {{'+', 1, 50}})
    space:commit_trans()
end;
--# setopt delimiter ''
space:select{100}

--# setopt delimiter ';'
for i = 1,1,1 do 
    space:start_trans()
    space:update({100}, {{'-', 1, 100}})
    space:rollback_trans()
end;
--# setopt delimiter ''
space:select{100}
 
space:start_trans()
space:update({100}, {{'+', 1, 100}})
space:rollback_trans()
