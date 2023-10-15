
local unittest = require "tests.unittest"
local db_users = require "tests.test_db_users"
local conf = require "conf"

local _M = {}

if conf.ut_enabled then
    _M.on_access = unittest.on_access
else
    _M.on_access = function (ctx, conf)
    end
end

return _M
