local json = require "cjson"
local conf = require 'conf'
local http = require "resty.http"
local httpc = http.new()

local _M = {}

--- 下面的 API 由 DatabaseApisHandler.cpp 提供

ngx.db_api = {}
ngx.db_api_col_names = {}

local function prepare_sql(path, sql)
    local key = path .. sql
    local stmtId = ngx.db_api[key]

    if stmtId ~= nil then
        return stmtId, ngx.db_api_col_names[key]
    end

    local res, err = httpc:request_uri(
        conf.server .. "/db-api/" .. path,
        {
            method = "POST",
            body = json.encode({action="prepare", sql=sql}),
        }
    )

    if err ~= nil then
        ngx.log(ngx.INFO, 'request_uri got no response, error: ', err)
        return nil, nil
    end

    if 200 == res.status then
        local body = json.decode(res.body)
        if body.result == "OK" then
            local stmtId = body["stmt-id"]
            local col_names = body["col-names"]

            ngx.log(ngx.INFO, "Db api prepared stmt-id: ", stmtId, ", sql: ", sql)

            ngx.db_api[key] = stmtId
            ngx.db_api_col_names[key] = col_names

            return stmtId, col_names
        else
            ngx.log(ngx.INFO, "Db api return result is not OK: ", body.result, " sql: ", sql, body.message)
        end
    else
        ngx.log(ngx.INFO, "Db api status code is: ", res.status, " sql: ", sql)
    end

    return nil, nil
end

local function run_sql(path, format, sql, args)
    local stmtId, col_names = prepare_sql(path, sql)

    if stmtId == nil then
        return nil
    end

    local res, err = httpc:request_uri(
        conf.server .. "/db-api/" .. path,
        {
            method="POST",
            body=json.encode({action="query", ["stmt-id"]=stmtId, format=format, args=args}),
        }
    )

    if err ~= nil then
        ngx.log(ngx.INFO, 'request_uri got no response, error: ', err)
        return nil
    end

    if 200 == res.status then
        local body = json.decode(res.body)
        if body.result == "OK" then
            return body.rows
        elseif body.result == "STMT-NOT-EXISTS" then
            -- 可能 API service 重启了，需要重新 prepare，让这次任务失败
            prepare_sql(path, sql)
        else
            ngx.log(ngx.INFO, "Db api return result is not OK: ", body.result, " sql: ", sql, body.message)
        end
    else
        ngx.log(ngx.INFO, "Db api status code is: ", res.status, " sql: ", sql)
    end

    return nil
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
    local res, err = httpc:request_uri(
        conf.server .. "/db-api/users/create-account",
        {
            method = "POST",
            body = json.encode(args),
        }
    )

    if err ~= nil then
        ngx.log(ngx.INFO, 'request_uri got no response, error: ', err)
        return nil
    end

    if 200 == res.status then
        local body = json.decode(res.body)
        if body.result == "OK" then
            return body.id
        else
            ngx.log(ngx.INFO, "Db api users/create-account return result is not OK: ", body.result)
        end
    else
        ngx.log(ngx.INFO, "Db api status code is: ", res.status)
    end

    return nil
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
    local res, err = httpc:request_uri(
        conf.server .. "/db-modify-api/" .. table_name,
        {
            method = "POST",
            body = json.encode({action=action, fields=fields, args=args}),
        }
    )

    if err ~= nil then
        ngx.log(ngx.INFO, 'request_uri got no response, error: ', err)
        return nil
    end

    if 200 == res.status then
        local body = json.decode(res.body)
        if body.result == "OK" then
            return body.id or -1
        else
            ngx.log(ngx.INFO, "Db api return result is not OK: ", body.result, " sql: ", sql, body.message)
        end
    else
        ngx.log(ngx.INFO, "Db api status code is: ", res.status, " sql: ", sql)
    end

    return nil
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

    local res, err = httpc:request_uri(
        conf.server .. "/lyrics-api/",
        {
            method = "POST",
            body = json.encode(body),
        }
    )

    if err ~= nil then
        ngx.log(ngx.INFO, 'request_uri got no response, error: ', err)
        return nil
    end

    if 200 == res.status then
        local body = json.decode(res.body)
        if body.result == "OK" then
            return true
        else
            ngx.log(ngx.INFO, "lyrics_api_request result is not OK: ", body.result, " sql: ", sql, body.message)
        end
    else
        ngx.log(ngx.INFO, "lyrics_api_request status code is: ", res.status, action)
    end

    return false
end

function _M.lyrics_file_delete(related_link)
    return lyrics_api_request('delete', {
        ["related-link"]=related_link
    })
end

return _M
