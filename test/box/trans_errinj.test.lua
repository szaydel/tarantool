errinj = require('errinj')
space = box.schema.create_space('trans_errjni')
space:create_index('primary', { type = 'hash' })

--# setopt delimiter ';'
function test_insert_failure()
    space:begin()
      space:insert{1}
      space:get{1}
      space:insert{2}
      space:insert{3}
      errinj.set("ERRINJ_WAL_IO", true)
    space:commit()
    errinj.set("ERRINJ_WAL_IO", false)
end;

function test_update_failure()
    space:begin()
      space:insert{100}
      space:insert{200}
    space:commit()
    space:begin()
      space:update(100, {{'=', 0, 1000}})
      space:update(200, {{'=', 0, 2000}})
    errinj.set("ERRINJ_WAL_IO", true)
    space:commit()
    errinj.set("ERRINJ_WAL_IO", false)
end;

function test_unique_constraint()
    space:begin()
      space:insert{10}
      space:insert{20}
      space:insert{20}
    space:commit()
end;

function test_delete()
    space:begin()
      space:insert{111}
      space:delete{222}
    space:commit()
    space:begin()
      space:insert{1111}
      space:delete{1111}
    space:commit()
end;

function test_empty_trans()
    space:begin()
    space:commit()
end;

test_insert_failure();
space:get{1};
space:get{2};
space:get{3};

test_update_failure();
space:get{100};
space:get{200};
space:get{1000};
space:get{2000};

test_unique_constraint();
space:get{10};
space:get{20};

errinj.set("ERRINJ_WAL_IO", false)
test_delete();
space:get{111};
space:get{1111};

test_empty_trans();
