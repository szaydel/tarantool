s = box.schema.create_space('spatial')
s:create_index('primary')
s:create_index('spatial', { type = 'rtree', parts = {1, 'num', 2, 'num', 3, 'num', 4, 'num'}})

s:insert{1,0,0,10,10}
s:insert{2,5,5,10,10}
s:insert{3,0,0,5,5}

-- select all records
s.index.spatial:select({}, {iterator = 'ALL'})
-- select records belonging to rectangle (0,0,5,5)
s.index.spatial:select({0,0,5,5}, {iterator = 'LE'})
-- select records strict belonging to rectangle (0,0,5,5)
s.index.spatial:select({0,0,5,5}, {iterator = 'LT'})
-- select records strict belonging to rectangle (4,4,10,10)
s.index.spatial:select({4,4,10,10}, {iterator = 'LT'})
-- select records containing point (5,5)
s.index.spatial:select({5,5}, {iterator = 'GE'})
-- select records containing rectangle (1,1,2,2)
s.index.spatial:select({1,1,2,2}, {iterator = 'GE'})
-- select records strict containing rectangle (0,0,5,5)
s.index.spatial:select({0,0,5,5}, {iterator = 'GT'})
-- select records overlapping rectangle (9,4,11,6)
s.index.spatial:select({9,4,11,6}, {iterator = 'OVERLAPS'})
-- select records with coordinates (0,0,5,5)
s.index.spatial:select({0,0,5,5}, {iterator = 'EQ'})
-- select neigbors of point (1,1)
s.index.spatial:select({1,1}, {iterator = 'NEIGHBOR'})

s:drop()
