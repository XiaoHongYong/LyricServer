local utils = require "utils"
local autoStaticUris = require "crintsoftStaticUris"
local contactus = require "contactus"

local staticPages = {
    ["/"] = 'music-player/default.html',
}

local urlHandlers = {
    ["/contactus"] = contactus,
}

local _M = {}

local function starts_with(str, start)
    return str:sub(1, #start) == start
end

_M.handleAll = function ()
    local ctx = {
        error = '',
        languages = utils.languages,
        language = utils.get_language(),
    }
    ngx.language = ctx.language

    local handler = urlHandlers[ngx.var.uri]
    if handler then
        handler(ctx)
        return
    end

    local templateFn = staticPages[ngx.var.uri]
    if templateFn then
        utils.render_template(templateFn, ctx)
    else
        utils.render_template(ngx.var.uri, ctx)
    end
end

local function mergeUriHandlers(handlers, additionHandlers)
    for k, v in pairs(additionHandlers) do
        handlers[k] = v
    end
end

mergeUriHandlers(staticPages, autoStaticUris)

return _M;