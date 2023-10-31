local template = require "resty.template"
local cjson = require 'cjson';

local _M = {}

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

local function read_file(path)
    local file, err = io.open(path, "rb")
    if not file then return nil, err end
    local content
    content, err = file:read "*a"
    file:close()
    return content, err
end

local function append_file(path, data)
    local file, err = io.open(path, "a+")
    if not file then return nil, err end
    local err = file:write(data)
    file:close()
    return err
end

local function read_json_file(path)
    local content, err = read_file(path)
    if not content then return nil, err end

    return cjson.decode(content), err
end

_M.read_file = read_file
_M.read_json_file = read_json_file
_M.append_file = append_file

local function get_fn(name)
    local root = ngx.config.prefix()
    if string.byte(root, -1) ~= string.byte('/') then
        root = root .. '/'
    end
    return root .. name
end
_M.get_fn = get_fn

local function get_template_fn(name)
    local root = ngx.var.template_root
    if string.byte(root, 1) ~= string.byte('/') then
        root = (ngx.config.prefix()) .. ngx.var.template_root
    end
    if string.byte(root, -1) ~= string.byte('/') then
        root = root .. '/'
    end
    return root .. name
end
_M.get_template_fn = get_template_fn

local function load_languages()
    local fn = get_template_fn('languages.json')
    local languages, err = read_json_file(fn)
    if not languages then
        error(err)
    end

    return languages
end

_M.languages = load_languages()

local function get_accept_language()
    local language = ngx.req.get_headers()['accept-language']
    if language == nil then
        return nil
    end

    local i = string.find(language, ';')
    local i2 = string.find(language, ',')
    if i2 < i then i = i2 end

    return string.sub(language, 1, i + 1)
end

_M.get_language = function ()

    local idx = string.find(ngx.var.host, '[.]')
    local language = string.sub(ngx.var.host, 1, idx - 1)
    if string.len(language) > 5 then
        language = get_accept_language()
    end

    language = string.lower(language)

    if language == nil or _M.languages[language] == nil then
        language = 'en'
    end

    return language
end

_M.dump_obj = dump_obj

_M.render_template = function (name, ctx)
    local template_root_org = ngx.var.template_root
    local lang = ngx.language
    if lang ~= nil then
        if string.byte(template_root_org, -1) ~= string.byte('/') then
            lang = '/' .. lang
        end
        ngx.var.template_root = ngx.var.template_root .. lang
    end

    template.render(name, ctx)

    ngx.var.template_root = template_root_org
end

return _M