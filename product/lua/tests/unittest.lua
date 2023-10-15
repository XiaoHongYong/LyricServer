
--
-- 此模块提供了运行 Lua unittest 的框架
-- * 可在代码中搜索调用 add_test_suite 的地方参考书写 unittest 代码
-- * 在浏览器中访问 /lua_ut 页面查看 uinittest 运行结果
--

local cjson = require 'cjson';

local _M = {}

local cur_logs = {}
local cur_logs_idx = 1

local test_suites = {}
local test_suites_results = {}
local cur_suite = nil
local cur_test = nil

local function dump_obj(obj)
    if type(obj) == "table" then
        local str = "{"
        for k, v in pairs(obj) do
            str = str .. dump_obj(k) .. ": " .. dump_obj(v) .. ", "
        end
        local metatable = getmetatable(obj)
            if metatable ~= nil and type(metatable.__index) == "table" then
            for k, v in pairs(metatable.__index) do
                str = str .. dump_obj(k) .. ": " .. dump_obj(v) .. ", "
            end
        end
        return str .. "}"
    elseif type(obj) == "string" then
        return '"' .. obj .. '"'
    else
        return tostring(obj)
    end
end

local function equal(v1, v2)
    local t1 = type(v1)
    local t2 = type(v2)
    if t1 ~= t2 then
        return false
    end

    if t1 ~= 'table' then
        return v1 == v2
    end

    local mt = getmetatable(v1)
    if mt and mt.__eq then
        return v1 == v2
    end

    local count = 0
    for k, v in pairs(v1) do
        count = count + 1

        local other = v2[k]
        if other == nil or not equal(v, other) then
            return false
        end
    end

    for k, v in pairs(v2) do
        count = count - 1
    end

    return count == 0
end

local function get_file_name(file)
    local name = string.gsub(file, "(.+/)([^/]+)$", "%2")
    if name == '' then
        name = file
    end
    return name
end

local function write_log(level, log, ...)
    local info = debug.getinfo(3)

    local name = info.name
    if name == nil then
        name = ''
    end

    local arg = {log, ...}
    local message = level .. ':' .. get_file_name(info.source) .. ':' .. name .. ':' .. info.currentline .. ': ';
    for i, v in ipairs(arg) do
        if v == nil then
            message = message .. ' nil'
        else
            if type(v) ~= 'string' then
                v = dump_obj(v)
            end
            message = message .. ' ' .. v
        end
    end

    cur_logs[cur_logs_idx] = message
    cur_logs_idx = cur_logs_idx + 1
end

local function start_test_suite(suite_name)
    cur_suite = {name=suite_name, cases={}}
    test_suites_results[#test_suites_results + 1] = cur_suite
end

local function start_test_case(case_name)
    cur_test = {name=case_name, logs={}, failed=false}
    cur_suite.cases[#cur_suite.cases + 1] = cur_test

    cur_logs = cur_test.logs
    cur_logs_idx = 1
end

local function run_test_suite(ctx, conf, suite_name, suite)
    start_test_suite(suite_name)

    local setup = nil
    local teardown = nil
    local keys = {}

    -- 获取待测试函数，分离 setup, teardown
    for key, value in pairs(suite) do
        if type(value) == 'function' then
            if key == 'setup' then setup = key
            elseif key == 'teardown' then teardown = key
            else table.insert(keys, key)
            end
        end
    end

    -- 按名称排序
    table.sort(keys)

    if setup ~= nil then table.insert(keys, 1, setup) end
    if teardown ~= nil then table.insert(keys, teardown) end

    for _, name in pairs(keys) do
        start_test_case(name)
        write_log('I', "Start test case: ", name)

        xpcall(function() suite[name](ctx, conf) end, function (err)
            write_log('E', 'Exception in test case: ', suite_name, '.', name)
            write_log('E', debug.traceback(err, 1))
            cur_test.failed = true
        end)
    end
end

local function run_all(ctx, conf)
    for i, suite in pairs(test_suites) do
        run_test_suite(ctx, conf, suite.name, suite.cases)
    end
end

function _M.get_function_args(fun)
	local args = {}
	local hook = debug.gethook()

	local argHook = function( ... )
		local info = debug.getinfo(3)
		if 'pcall' ~= info.name then
			return
		end

		for i = 1, 100 do
			local name, value = debug.getlocal(2, i)
			if name == nil or '(*temporary)' == name then
				debug.sethook(hook)
				return
			end

			table.insert(args, name)
		end
	end

	debug.sethook(argHook, "c")
	pcall(fun)

	debug.sethook(hook)

	return args
end

function _M.log(log, ...)
    if cur_test == nil then
        return
    end

    local args = {log, ...}

    write_log('I', unpack(args))
end

local function failed(log, ...)
    if cur_test == nil then
        return
    end

    cur_test.failed = true

    local args = {log, ...}

    write_log('E', unpack(args))
end

_M.failed = failed

function _M.ASSERT_EQ(v1, v2)
    write_log('I', 'ASSERT_EQ: ', v1, ', ', v2)
    if not equal(v1, v2) then
        failed(v1, ' is NOT EQUAL to ', v2)
    end
end

function _M.ASSERT_NIL(v)
    write_log('I', 'ASSERT_NIL: ', v)
    if v ~= nil then
        failed(v, 'is not nil')
    end
end

function _M.ASSERT_NOT_NIL(v)
    write_log('I', 'ASSERT_NOT_NIL: ', v)
    if v == nil then
        failed('nil not expected')
    end
end

function _M.ASSERT_TRUE(v)
    write_log('I', 'ASSERT_TRUE: ', v)
    if not v then
        failed(v, 'is not true')
    end
end

function _M.ASSERT_FALSE(v)
    write_log('I', 'ASSERT_FALSE: ', v)
    if v then
        failed(v, 'is not false')
    end
end

function _M.add_test_suite(suite_name, suite)
    test_suites[#test_suites + 1] = {name=suite_name, cases=suite}
end

function _M.on_access(ctx, conf)
    run_all(ctx, conf)
    ngx.say(cjson.encode(test_suites_results))
end

return _M