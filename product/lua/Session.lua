local aes = require "resty.aes"
local bit = require "bit"
local conf = require 'conf'

local _M = {}

local aesSession = aes:new(conf.key)

local SESSION_TYPE_LOGIN = 'b'
local SESSION_TYPE_RESET_PASSWORD = 'r'

local SESSION_LOGIN_VERSION = 'a'
local SESSION_RESET_PASSWORD_VERSION = 'a'

local function signedInt32ToUnsigned(n)
    if n < 0 then
        return 0x100000000 + n
    end
    return n
end

local function uint8ToBytesStr(n)
    assert(n >= 0 and n <= 255)
    return string.char(n)
end

local function uint8FromBytesStr(n)
    return string.byte(s, 1, 1);
end

local function uint32ToBytesStr(n)
    local n0 = bit.band(n, 0xff)
    local n1 = bit.band(bit.rshift(n, 8), 0xff)
    local n2 = bit.band(bit.rshift(n, 16), 0xff)
    local n3 = bit.band(bit.rshift(n, 24), 0xff)

    return string.char(n0, n1, n2, n3)
end

local function uint32FromBytesStr(s, defValueIfFailed)
    if string.len(s) ~= 4 then
        ngx.log(ngx.ERR, 'Input string length is NOT 4 bytes: ', string.len(s))
        return defValueIfFailed
    end

    local n0, n1, n2, n3 = string.byte(s, 1, 4)
    return signedInt32ToUnsigned(bit.bor(bit.lshift(n3, 24),
                                 bit.lshift(n2, 16),
                                 bit.lshift(n1, 8),
                                 n0))
end

local function encode_base64_safe(s)
    s = ngx.encode_base64(s)
    return s:gsub('+', '.')
end

local function decode_base64_safe(s)
    s = string.gsub(s, '[.]', '+')
    return ngx.decode_base64(s)
end

local function encryptWithSessionKey(type, version, user_id)
    local random = require "resty.random"

    local randBytes = random.bytes(4)

    user_id = uint32ToBytesStr(user_id)
    local encryptTime = uint32ToBytesStr(os.time())

    local encryptedSessionId = aesSession:encrypt(type .. version .. user_id .. encryptTime .. randBytes)
    return type .. version .. encode_base64_safe(encryptedSessionId)
end

local function decryptWithSessionKey(sessionInfo, type, version)
    local typeVersion = type .. version
    local ret = {}
    if sessionInfo == '' then
        return ret
    end

    if string.sub(sessionInfo, 1, 2) ~= typeVersion then
        ngx.log(ngx.ERR, 'session Version NOT match.', sessionInfo, typeVersion)
        return ret
    end
    sessionInfo = string.sub(sessionInfo, 3)

    sessionInfo = decode_base64_safe(sessionInfo)
    if sessionInfo == nil then
        ngx.log(ngx.ERR, 'Failed to decode base64 of sessionInfo.', sessionInfo)
        return ret
    end

    sessionInfo = aesSession:decrypt(sessionInfo)
    if sessionInfo == nil then
        ngx.log(ngx.ERR, 'Failed to decrypt: ', sessionInfo)
        return ret
    end

    if string.sub(sessionInfo, 1, 2) ~= typeVersion then
        ngx.log(ngx.ERR, 'session inner Version NOT match.')
        return ret
    end

    return {
        user_id = uint32FromBytesStr(string.sub(sessionInfo, 3, 6)),
        time = uint32FromBytesStr(string.sub(sessionInfo, 7, 10)),
    }
end

function _M.encryptSessionInfo(user_id)
    return encryptWithSessionKey(SESSION_TYPE_LOGIN, SESSION_LOGIN_VERSION, user_id)
end

function _M.decryptSessionInfo(sessionInfo)
    return decryptWithSessionKey(sessionInfo, SESSION_TYPE_LOGIN, SESSION_LOGIN_VERSION).user_id
end


function _M.encryptResetPasswordSession(user_id)
    return encryptWithSessionKey(SESSION_TYPE_RESET_PASSWORD, SESSION_RESET_PASSWORD_VERSION, user_id)
end

function _M.decryptResetPasswordSession(sessionInfo)
    return decryptWithSessionKey(sessionInfo, SESSION_TYPE_RESET_PASSWORD, SESSION_RESET_PASSWORD_VERSION)
end

local function assert_equal(a, b)
    if a ~= b then
        ngx.log(ngx.ERR, 'assert_equal failed: ', a, ' ~= ', b)
    end
    assert(a == b)
end

function _M.test()
    local id = 0xa0a1b2c3
    local sessionInfo = _M.encryptSessionInfo(id)
    ngx.log(ngx.ERR, 'Session ID: ' .. sessionInfo)

    local id2 = _M.decryptSessionInfo(sessionInfo)
    ngx.log(ngx.ERR, id, ':', id2)
    assert_equal(id, id2)

    id = 0xa0a1b21
    sessionInfo = _M.encryptResetPasswordSession(id)
    ngx.log(ngx.ERR, 'Session ID: ' .. sessionInfo)

    local info = _M.decryptResetPasswordSession(sessionInfo)
    ngx.log(ngx.ERR, id, ':', info.user_id)
    assert_equal(id, info.user_id)
end

return _M