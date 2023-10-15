local resty_md5 = require "resty.md5"
local db_api = require "database_api"

local function hashUserPassword(password)
    local md5 = resty_md5:new()
    md5:update(password)
    md5:update('@crintsoft.com')
    return ngx.encode_base64(md5:final())
end

local function hash_minilyrics_client_password(password)
    local md5 = resty_md5:new()
    md5:update(password)
    local digest = md5:final()
    digest = string.sub(digest, 5, 12)

    local str = require "resty.string"
    return string.upper(str.to_hex(digest))
end

local _M = {}

_M.hashUserPassword = hashUserPassword
_M.hash_minilyrics_client_password = hash_minilyrics_client_password

function _M.get_user_by_id(user_id)
    return db_api.users_r1("SELECT * FROM users WHERE id=?", {user_id})
end

function _M.get_user_by_email(email)
    return db_api.users_r1("SELECT * FROM users WHERE Email=?", {email})
end

function _M.signin_user(user_name, password)
    local passwordHash = hashUserPassword(password)

    local user = db_api.users_r1("SELECT id, UserName, Email, PasswordHash FROM users WHERE UserName=? or Email=?", {user_name, user_name})
    if user ~= nil and user.PasswordHash == passwordHash then
        return user.id
    end

    return nil
end

local function string_trim(s)
    return s:match("^%s*(.-)%s*$")
end

function _M.create_account(ctx)
    ctx.username = string_trim(ctx.username)
    ctx.email = string_trim(ctx.email)
    if string.len(ctx.username) < 3 then
        ctx.error = 'Username must be at least 3 chars.'
        return false;
    end

    if #ctx.email < 6 or string.find(ctx.email, '@') == nil then
        ctx.error = 'Invalid email address format.'
        return false;
    end

    local username = ctx.username
    local email = ctx.email

    local user = db_api.users_r1("SELECT id, UserName, Email, PasswordHash FROM users WHERE UserName=? or Email=?", {username, email})
    if user ~= nil then
        ctx.error = 'User with the same name or email exists already.'
        return false;
    end

    ctx.user_id = db_api.users_create("MLPasswordHash,PasswordHash,UserName,Email,CreateDate,LastLoginDate,LastPasswordChangedDate",
        { hash_minilyrics_client_password(ctx.password), hashUserPassword(ctx.password),
        username, email, ngx.utctime(), ngx.utctime(), ngx.utctime() }
    )

    if ctx.user_id == nil then
        ctx.error = 'Failed to create account. Please contact us to solve the problem.'
        return false
    end

    return true
end

function _M.remove_account(user_id)
    if user_id == nil then
        return 'user-id is expected.'
    end

    return db_api.users_delete(user_id)
end

function _M.changeEmail(user_id, newEmail)
    if user_id == nil then
        return 'user-id is expected.'
    end

    local ret = db_api.users_update("Email", {newEmail, user_id})

    if ret ~= nil then
        return ''
    end

    ngx.log(ngx.ERR, 'Failed to change email address', user_id, ', ', newEmail)

    return 'Failed to change email address.'
end

function _M.changePassword(user_id, password)
    if user_id == nil then
        return 'user-id is expected.'
    end

    local PasswordHash = hashUserPassword(password)
    local MLPasswordHash = hash_minilyrics_client_password(password)

    local ret = db_api.users_update("PasswordHash,MLPasswordHash", {PasswordHash, MLPasswordHash, user_id})

    if ret ~= nil then
        return ''
    end

    ngx.log(ngx.ERR, 'Failed to change password')

    return 'Failed to change password.'
end

return _M