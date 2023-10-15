local template = require "resty.template"
local autoStaticUris = require "crintsoftStaticUris"

local staticPages = {
    ["/"] = 'minilyrics/default.html',
}

local function starts_with(str, start)
    return str:sub(1, #start) == start
end

local function handleAll()

    local templateFn = staticPages[ngx.var.uri]
    if templateFn then
        template.render(templateFn, ctx)
    else
        ngx.say('404 not found: ' .. ngx.var.uri)
    end
end

local function mergeUriHandlers(handlers, additionHandlers)
    for k, v in pairs(additionHandlers) do
        handlers[k] = v
    end
end

mergeUriHandlers(staticPages, autoStaticUris)
handleAll()
