-- fiber_timeout.lua

--
-- timeout = require 'fiber.timeout'
--
-- local status, result = timeout(delay, myfunction, arg1, ...)
--
-- if not status then
--    return 'Timeout reached'
-- else
--    return result
-- end
--
-- local pstatus, tstatus, result = pcall(timeout, delay, myfunction, ...)
-- local tstatus, pstatus, result = timeout(delay, pcall, myfunction, ...)
--
-- Note: if an exception is thrown after timeout, there is now way to catch it.
-- Text of the exception will be placed to tarantool's log.
--

local fiber = require 'fiber'
local log   = require 'log'
local errno = require 'errno'

local function timeout(timeout, foo, ...)

    if timeout == nil then
        return foo(...)
    end
    local is_run = true
    local res = nil

    local f = fiber.self()

    fiber.create(
        function(...)
            fiber.name("timeout")
            res = { pcall(foo, ...) }

            if not is_run then
                if not res[1] then
                    log.warn("Exception after timeout: %s", res[2])
                end
                return
            end

            is_run = false
            if fiber.status(f) == 'suspended' then
                f:wakeup()
            end
        end,
        ...
    )

    local started = fiber.time()
    local finished

    fiber.sleep(timeout)
    is_run = false

    -- timeout
    if res == nil then
        errno(errno.ETIMEDOUT)
        return false
    end

    if not res[1] then
        box.error(box.error.PROC_LUA, res[2])
    end

    return unpack(res)
end


local function timeout_call(self, ...)
    return timeout(...)
end

local res = {}
setmetatable(res, { __index = { timeout = timeout }, __call = timeout_call })
return res
