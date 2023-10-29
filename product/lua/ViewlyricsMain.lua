local viewUser = require "ViewUser"
local viewLyrics = require "ViewLyrics"
local session = require "Session"
local utils = require "utils"

local _M = {}

local function home(ctx, user_id)
    utils.render_template("home.html", ctx)
end

local uriHandlers = {
    ['/'] = home,
}

local function starts_with(str, start)
    return str:sub(1, #start) == start
end

_M.handleAll = function ()
    if ngx.var.uri == '/d' then
        -- for debug testing
        local language = utils.get_language()

        ngx.say(language)
        return
    end

    local user_id = nil
    local sessionInfo = ngx.var.cookie_session
    if sessionInfo ~= nil then
        user_id = session.decryptSessionInfo(sessionInfo)
    end

    local handler = uriHandlers[ngx.var.uri]
    if handler then
        local ctx = {
            error = '',
            languages = utils.languages,
            language = utils.get_language(),
        }
        ngx.language = ctx.language
        if user_id ~= nil then
            ctx.isSignedIn = 'true'
        else
            ctx.isSignedIn = 'false'
        end

        if starts_with(ngx.var.uri, '/user/') and ngx.var.uri ~= '/user/signin' and
            ngx.var.uri ~= '/user/signup' and ngx.var.uri ~= '/user/password_reset' and user_id == nil then
            ngx.redirect('/user/signin?redirect=' .. ngx.var.uri)
        end

        handler(ctx, user_id)
    else
        ngx.say('404 not found: ' .. ngx.var.uri)
    end
end

local function mergeUriHandlers(handlers, additionHandlers)
    for k, v in pairs(additionHandlers) do
        handlers[k] = v
    end
end

mergeUriHandlers(uriHandlers, viewUser)
mergeUriHandlers(uriHandlers, viewLyrics)

return _M;