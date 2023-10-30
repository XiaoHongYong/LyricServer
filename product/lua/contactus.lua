local cjson = require "cjson"
local utils = require "utils"
local locale = require "locale"

local function contactus(ctx)
    ctx.info = ''
    ctx.error = ''

    if ngx.var.request_method == "POST" then
        -- Check inputed email
        ngx.req.read_body()
        local args, err = ngx.req.get_post_args()
        if args == nil then
            ctx.error = "Request body is required."
        else
            local data = {
                email = args.email,
                title = args.title,
                message = args.message
            }

            utils.append_file(utils.get_fn('database/contactus-messages.json'), cjson.encode(data) .. '\n')
        end

        ctx.info = locale._TLM('Thanks, your message has been sent.')
    end

    utils.render_template("contactus.html", ctx)
end

return contactus