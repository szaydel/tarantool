box.cfg.transaction_limit
box.cfg{transaction_limit=10}
box.cfg.transaction_limit
space = box.schema.create_space('tweedledum')
space:create_index('primary')

--# setopt delimiter ';'
function do_transaction()
    box.begin()
    for i = 1,100,1 do
        space:insert{i}
    end
    box.commit()
end;

--# setopt delimiter ''
do_transaction()
space:len()
box.cfg{transaction_limit=0}
box.cfg.transaction_limit
do_transaction()
space:len()
space:drop()
