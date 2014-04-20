box.cfg.transaction_limit
box.cfg{transaction_limit=10}
box.cfg.transaction_limit
space = box.schema.create_space('tweedledum', { id = 0 })
space:create_index('primary', { type = 'hash' })

--# setopt delimiter ';'
function do_transaction()
    space:start_trans()
    for i = 1,100,1 do space:insert{i} end
    space:commit_trans()
end;

do_transaction();
space:len();
box.cfg{transaction_limit=0};
box.cfg.transaction_limit;
do_transaction();
space:len();
space:drop();
