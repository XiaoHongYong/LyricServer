local utils = require "utils"
local _M = {}

local function load_translates()
    local translates = {}

    for lang, _ in pairs(utils.languages) do
        local trans, err = utils.read_json_file(utils.get_template_fn(lang .. '.json'))
        if not trans then
            error(err)
        end

        translates[lang] = trans
    end

    return translates
end

local translates = load_translates()

_M._TLM = function (text)
    local language = ngx.language

    return translates[ngx.language][text] or text
end

return _M