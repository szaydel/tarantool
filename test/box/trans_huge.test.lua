space = box.schema.create_space('huge')
space:create_index('primary', { type = 'hash' })

trans_size = 1024
n_trans = 1024

--# setopt delimiter ';'
function transaction(tid)
    local id = tid*trans_size
    box.begin()
    for i = 1,trans_size,1 do 
        space:insert{id} 
        id = id + 1
    end
    box.commit()
end;
    
for tid = 1,n_trans,1 do 
    transaction(tid)
end;

space:len()
