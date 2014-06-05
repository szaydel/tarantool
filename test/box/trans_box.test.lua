space = box.schema.create_space('account')
space:create_index('primary', { type = 'hash' })
for i = 1,100,1 do space:insert({i, 100}) end
--# setopt delimiter ';'
for i = 1,99,1 do
    space:begin()
    space:update({i}, {{'-', 1, 50}})
    space:update({i+1}, {{'+', 1, 50}})
    space:commit()
end;
--# setopt delimiter ''
space:select{100}

--# setopt delimiter ';'
for i = 1,1,1 do
    space:begin()
    space:update({100}, {{'-', 1, 100}})
    space:rollback()
end;
--# setopt delimiter ''
space:select{100}

space:drop()
