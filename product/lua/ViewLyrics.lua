local template = require "resty.template"
local dbLyrics = require("db_lyrics")
local cjson = require "cjson"
local _TLM = require("locale")._TLM


local _M = {}


_M['/api/usr/lyrics/uploaded'] = function (ctx, user_id)
    if user_id == nil then
        ngx.say(cjson.encode({
            error = 'Invalid session information, please sign out, then sign in again.'
        }))
    else
        local offset = ngx.req.get_uri_args()['offset']
        if offset == nil then
            offset = 0
        end

        local rows = dbLyrics.getLyricsByUserId(user_id, offset)

        ngx.say(cjson.encode({
            columns = dbLyrics.getLyricsByUserIdColumns(),
            rows = rows,
        }))
    end
end

_M['/api/user/lyrics/delete'] = function (ctx, user_id)
    if user_id == nil then
        ngx.say(cjson.encode({
            error = 'Invalid session information, please sign out, then sign in again.'
        }))
    else
        ngx.req.read_body()
        local data = ngx.req.get_body_data()
        if data ~= nil then
            local args = cjson.decode(data)
            if args ~= nil and args.id ~= nil then
                if not dbLyrics.deleteLyricsByIdAndUserId(args.id, user_id) then
                    ctx.error = _TLM("Lyrics were NOT found.")
                end
            end
        end

        ngx.say(cjson.encode(ctx))
    end
end


return _M