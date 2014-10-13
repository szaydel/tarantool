
timeout = require 'fiber.timeout'
fiber = require 'fiber'
fio = require 'fio'


--# setopt delimiter ';'
function test(sleep, retval, exception)
    fiber.sleep(sleep)
    if retval == nil then
        box.error(box.error.PROC_LUA, exception)
    end
    return retval
end;
--# setopt delimiter ''


test(0.1, 1)
test(0.1, nil, 'test exception')

timeout(0.1, test, 0.2, 123)
timeout(0.1, test, 0.01, 123)

timeout(0.1, test, 0.2, nil, 'test exception')
timeout(0.1, test, 0.01, nil, 'test exception')

pcall(timeout, 0.1, test, 0.01, nil, 'test exception')
pcall(timeout, 0.01, test, 0.1, nil, 'test exception')

fh = fio.open('tarantool.log', 'O_RDWR')
fh:seek(0, 'SEEK_END') > 0

timeout(0.1, pcall, test, 0.01, nil, 'test exception')
timeout(0.01, pcall, test, 0.1, nil, 'test exception')


fiber.sleep(.1)

string.match(fh:read(4096), 'Exception after timeout') ~= nil
fh:close()
