local json = require "cjson"
local conf = require 'conf'
local http = require "resty.http"

local _M = {}

--- 下面的 API 由 DatabaseApisHandler.cpp 提供

ngx.db_api = {}
ngx.db_api_col_names = {}

local function api_request(path, body)
    local httpc = http.new()
    local ok, err = httpc:connect({
        scheme = "http",
        host = conf.server,
        port = conf.port,
    })
    if not ok then
        ngx.log(ngx.ERR, "Failed to connect to server: ", conf.server, ", port: ", conf.port)
        return nil
    end

    body = json.encode(body)
    local res, err = httpc:request({
        method = "POST",
        path = path,
        body = body
    })
    if not res then
        ngx.log(ngx.ERR, "api_request failed: ", err, ", path: ", path, ", body: ", body)
        return nil
    end

    if 200 == res.status then
        local res_body, err = res:read_body()
        if res_body then
            return json.decode(res_body)
        else
            ngx.log(ngx.ERR, "api_request failed to read response body", err)
        end
    else
        ngx.log(ngx.ERR, "api_request response status code is: ", res.status, " sql: ", sql)
    end

    return nil
end

-- return result == 'OK' only
local function api_request_rok(path, req_body)
    local body = api_request(path, req_body)
    if body then
        if body.result == "OK" then
            return body
        else
            ngx.log(ngx.INFO, "api_request result is not OK: ", body.result, " sql: ", sql, body.message)
        end
    end

    return nil
end

local function db_api_key(path, sql)
    return path .. sql
end

local function prepare_sql(path, sql)
    local key = db_api_key(path, sql)
    local stmtId = ngx.db_api[key]

    if stmtId ~= nil then
        return stmtId, ngx.db_api_col_names[key]
    end

    local body = api_request_rok("/db-api/" .. path, {
            action="prepare",
            sql=sql,
        }
    )
    if not body then
        return nil, nil
    end

    local stmtId = body["stmt-id"]
    local col_names = body["col-names"]

    ngx.log(ngx.INFO, "Db api prepared stmt-id: ", stmtId, ", sql: ", sql)

    ngx.db_api[key] = stmtId
    ngx.db_api_col_names[key] = col_names

    return stmtId, col_names
end

local function _run_sql(path, format, sql, args, retry_count)
    local stmtId, col_names = prepare_sql(path, sql)

    if stmtId == nil then
        return nil
    end

    -- ngx.log(ngx.INFO, 'request_uri: ', conf.server .. "/db-api/" .. path)

    local body = api_request("/db-api/" .. path, {
        action="query",
        ["stmt-id"]=stmtId, format=format, args=args
    })
    if not body then
        return nil
    end

    if body.result == "OK" then
        return body.rows
    elseif body.result == "STMT-NOT-EXISTS" then
        -- 可能 API service 重启了，需要重新 prepare
        ngx.db_api[db_api_key(path, sql)] = nil
        if retry_count > 0 then
            return _run_sql(path, format, sql, args, retry_count - 1)
        end
    else
        ngx.log(ngx.INFO, "Db api return result is not OK: ", body.result, " sql: ", sql, body.message)
    end

    return nil
end

local function run_sql(path, format, sql, args)
    return _run_sql(path, format, sql, args, 1)
end

function _M.users_column_names(sql)
    local stmtId, col_names = prepare_sql('users', sql)
    return col_names
end

function _M.users(sql, args)
    return run_sql('users', 'dict', sql, args)
end

function _M.users_r1(sql, args)
    local rows = run_sql('users', 'dict', sql, args)
    if rows ~= nil then
        return rows[1]
    end

    return nil
end

function _M.users_r1_1(sql, args)
    local rows = run_sql('users', 'array', sql, args)
    if rows ~= nil and rows[1] ~= nil then
        return rows[1][1]
    end

    return nil
end

function _M.users_create_account(args)
    local body = api_request_rok("/db-api/users/create-account", args)
    if not body then
        return nil
    end

    return body.id
end

function _M.lyrics_column_names(sql)
    local stmtId, col_names = prepare_sql('lyrics', sql)
    return col_names
end

function _M.lyrics(sql, args)
    return run_sql('lyrics', 'dict', sql, args)
end

function _M.lyrics_r1(sql, args)
    local rows = run_sql('lyrics', 'dict', sql, args)
    if rows ~= nil then
        return rows[1]
    end

    return nil
end

function _M.lyrics_array(sql, args)
    return run_sql('lyrics', 'array', sql, args)
end

function _M.lyrics_r1_1(sql, args)
    local rows = run_sql('lyrics', 'array', sql, args)
    if rows ~= nil and rows[1] ~= nil then
        return rows[1][1]
    end

    return nil
end

--- 下面的 API 由 DbModifyHandler.cpp 提供

local function db_modify_api_request(table_name, action, fields, args)
    local body = api_request_rok("/db-modify-api/" .. table_name, {
        action=action,
        fields=fields,
        args=args,
    })
    if not body then
        return nil
    end

    return body.id or -1
end

function _M.users_create(fields, args)
    return db_modify_api_request('users', 'create', fields, args)
end

function _M.users_update(fields, args)
    return db_modify_api_request('users', 'update', fields, args)
end

function _M.users_delete(id)
    return db_modify_api_request('users', 'delete', "id", { id })
end

function _M.lyrics_delete_by_id(id)
    return db_modify_api_request('lyrics', 'delete', "id", { id })
end

--- 下面的 API 由 LyricsFileHandler.cpp 提供

local function lyrics_api_request(action, args)
    local body = {action=action}
    for k,v in pairs(args) do
        body[k] = v
    end

    local res_body = api_request_rok("/lyrics-api/", body)
    return res_body ~= nil
end

function _M.lyrics_file_delete(related_link)
    return lyrics_api_request('delete', {
        ["related-link"]=related_link
    })
end

return _M
