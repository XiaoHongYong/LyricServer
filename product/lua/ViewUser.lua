local template = require "resty.template"
local dbUsers = require("db_users")
local session = require "Session"
local cjson = require "cjson"
local utils = require "utils"
local _TLM = require("locale")._TLM


local _M = {}

_M['/user/signin'] = function (ctx, user_id)
    if ngx.var.request_method == "POST" then
        ngx.req.read_body()
        local args, err = ngx.req.get_post_args()
        if args == nil then
            ctx.error = "Request body is required."
        else
            local user_id = dbUsers.signin_user(args.user, args.password)
            if user_id ~= nil then
                local sessionInfo = session.encryptSessionInfo(user_id)
                local expireTime = ngx.http_time(os.time() + 365 * 24 * 60 * 60)
                local sessionCookie = 'session=' .. sessionInfo  .. '; path=/; HttpOnly; Expires: ' .. expireTime
                ngx.header['Set-Cookie'] = sessionCookie

                local redirect = args.redirect
                if redirect == nil or redirect == '' then
                    redirect = '/user/profile'
                end
                ngx.redirect(redirect)
                return;
            else
                ctx.error = _TLM("Wrong user name/email or password.")
            end
        end
    else
        ctx.redirect = ngx.req.get_uri_args().redirect or ''
    end

    utils.render_template("user/signin.html", ctx)
end

_M['/user/Login.aspx'] = _M['/user/signin']

_M['/user/signout'] = function (ctx, user_id)
    local sessionCookie = 'session=; path=/; Expires: Thu, 01 Jan 1970 00:00:00 GMT; HttpOnly'
    ngx.header['Set-Cookie'] = sessionCookie
    ngx.redirect('/user/signin')
end

_M['/user/signup'] = function (ctx, user_id)
    if ngx.var.request_method == "POST" then
        ngx.req.read_body()
        local args, err = ngx.req.get_post_args()
        if args == nil then
            ctx.error = "Request body is required."
        else
            ctx.username = args.username or ''
            ctx.email = args.email or ''
            ctx.password = args.password or ''

            if dbUsers.create_account(ctx) then
                local sessionInfo = session.encryptSessionInfo(ctx.user_id)
                local expireTime = ngx.http_time(os.time() + 365 * 24 * 60 * 60)
                local sessionCookie = 'session=' .. sessionInfo  .. '; path=/; HttpOnly; Expires: ' .. expireTime
                ngx.header['Set-Cookie'] = sessionCookie

                local redirect = args.redirect
                if redirect == nil or redirect == '' then
                    redirect = '/user/profile'
                end
                ngx.redirect(redirect)
                return;
            end
        end
    else
        ctx.redirect = ngx.req.get_uri_args().redirect or ''
        ctx.username = ''
        ctx.email = ''
        ctx.password = ''
    end

    utils.render_template("user/signup.html", ctx)
end

_M['/user/signup.aspx'] = _M['/user/signup']

local function sendPasswordResetEmail(ctx, email, user_id)
    mailSender = require "MailSender"

    local resetKey = session.encryptResetPasswordSession(user_id)

    if mailSender.sendMail(email, 'support@crintsoft.com', _TLM('[ViewLyrics] Please reset your password'), 
        '\n' ..
_TLM('We heard that you lost your ViewLyrics.com password. Sorry about that!') .. '\n\n' ..
_TLM("But don’t worry! You can use the following link to reset your password:") ..'\n\n' ..
string.format('https://viewlyrics.com/user/password_reset?key=%s\n\n', resetKey) ..
_TLM("If you don’t use this link within 3 hours, it will expire. To get a new password reset link, visit") ..
" https://viewlyrics.com/user/password_reset\n\n" ..
_TLM("Thanks,") ..
"\nViewLyrics.com") then
        ctx.info = _TLM("Check your email for a link to reset your password. If it doesn’t appear in a few minutes, check your spam folder.")
    else
        ctx.error = _TLM('Failed to send email, please contact us to solve this problem.')
    end
end

_M['/user/password_reset'] = function (ctx)
    ctx.start_reset = true
    ctx.reset_done = false
    ctx.info = ''
    ctx.error = ''

    local key = ngx.req.get_uri_args().key
    if key == nil then
        -- Enter email stage.
        if ngx.var.request_method == "POST" then
            -- Check inputed email
            ngx.req.read_body()
            local args, err = ngx.req.get_post_args()
            if args == nil then
                ctx.error = "Request body is required."
            else
                ctx.email = args.email or ''
                local info = dbUsers.get_user_by_email(ctx.email)
                if info == nil then
                    ctx.error = _TLM("Can't find that email, sorry.")
                else
                    sendPasswordResetEmail(ctx, ctx.email, info.id)
                end
            end
        end
    else
        local info = session.decryptResetPasswordSession(key)
        if info.user_id == nil then
            ctx.error = _TLM('It looks like you clicked on an invalid password reset link. Please try again.')
        else
            local now = os.time()
            if now < info.time or now - info.time > 30 * 60 then
                ctx.error = _TLM('The link can only be used in 30 minutes, please try again.')
            else
                -- Key is verifed, OK to proceed to "reset password".
                -- ！！！ 这里有安全漏洞，因为链接中的验证码 key 可以多次使用，而且是放在 url 中的，很容易通过 referer 泄露到
                -- 第三方的 JS 中。
                ctx.start_reset = false
                if ngx.var.request_method == "POST" then
                    -- Change the password.
                    ngx.req.read_body()
                    local args, err = ngx.req.get_post_args()
                    if args == nil then
                        ctx.error = err or "Request body is required."
                    else
                        if args.password == '' or args.password == nil then
                            ctx.error = _TLM('Please input new password.')
                        else
                            ctx.error = dbUsers.changePassword(info.user_id, args.password)
                            if ctx.error == '' then
                                ctx.reset_done = true
                                ctx.info = _TLM('You have reset your password successfully, you can sign in now.')
                            end
                        end
                    end
                end
            end
        end
    end

    ctx.start_reset = cjson.encode(ctx.start_reset)
    ctx.reset_done = cjson.encode(ctx.reset_done)

    utils.render_template("user/password_reset.html", ctx)
end

_M['/user/profile'] = function (ctx, user_id)
    local info = dbUsers.get_user_by_id(user_id)
    if info == nil then
        ctx.error = _TLM('Invalid session information, please sign out, then sign in again.')
        ngx.log(ngx.INFO, 'Invalid session:', user_id)
    else
        if ngx.var.request_method == "POST" then
            ctx.error = "Invalid input parameters."

            ngx.req.read_body()
            local data = ngx.req.get_body_data()
            if data ~= nil then
                local args = cjson.decode(data)
                if args ~= nil and args.email ~= nil and args.password ~= nil and args.email ~= '' and args.password ~= '' then
                    if info.Email ~= args.email then
                        if string.lower(info.Email) == string.lower(args.email) then
                            ctx.error = dbUsers.changeEmail(user_id, args.email)
                        else
                            if dbUsers.hashUserPassword(args.password) == info.PasswordHash then
                                if dbUsers.get_user_by_email(args.email) == nil then
                                    ctx.error = dbUsers.changeEmail(user_id, args.email)
                                else
                                    ctx.error = _TLM("An account is already associated with the email address: ") .. args.email
                                end
                            else
                                ctx.error = _TLM("Incorrect password.")
                            end
                        end
                    else
                        ctx.error = _TLM("Email address hasn't been changed.")
                    end    
                end
            end

            ngx.say(cjson.encode({
                error = ctx.error,
            }))
            return;
        end

        if info ~= nil then
            ctx.profile = cjson.encode({
                name = info.UserName,
                email = info.Email,
                createDate = info.CreateDate,
            })
        end
    end

    utils.render_template("user/profile.html", ctx)
end

_M['/user/change-password'] = function (ctx, user_id)
    local info = dbUsers.get_user_by_id(user_id)
    if info == nil then
        ctx.error = _TLM('Invalid session information, please sign out, then sign in again.')
    else
        if ngx.var.request_method == "POST" then
            ctx.error = "Invalid input parameters."

            ngx.req.read_body()
            local data = ngx.req.get_body_data()
            if data ~= nil then
                local args = cjson.decode(data)
                if args ~= nil and args.password_old ~= nil and args.password ~= nil and args.password_old ~= '' and args.password ~= '' then
                    if dbUsers.hashUserPassword(args.password_old) == info.PasswordHash then
                        assert(dbUsers.hash_minilyrics_client_password(args.password_old) == info.MLPasswordHash)

                        ctx.error = dbUsers.changePassword(user_id, args.password)
                    else
                        ctx.error = _TLM("Incorrect password.")
                    end
                end
            end

            ngx.say(cjson.encode({
                error = ctx.error,
            }))
            return;
        end

        if info ~= nil then
            ctx.profile = cjson.encode({
                name = info.UserName,
                email = info.Email,
                createDate = info.CreateDate,
            })
        end
    end

    utils.render_template("user/change-password.html", ctx)
end

_M['/user/uploaded-lyrics'] = function (ctx, user_id)
    local dbLyrics = require "db_lyrics"

    ctx.lyricsCount = dbLyrics.getCountLyricsByUserId(user_id)
    ctx.pageCount = math.ceil(ctx.lyricsCount / 50)
    if ctx.pageCount == 0 then
        ctx.pageCount = 1
    end

    ctx.curPage = ngx.req.get_uri_args().page
    if ctx.curPage == nil or ctx.curPage == '' then
        ctx.curPage = 1
    else
        ctx.curPage = tonumber(ctx.curPage);
        if ctx.curPage <= 0 then
            ctx.curPage = 1
        end
        if ctx.curPage > ctx.pageCount then
            ctx.curPage = ctx.pageCount
        end
    end

    ctx.offset = (ctx.curPage - 1) * 50

    ctx.rows = cjson.encode(dbLyrics.getLyricsByUserId(user_id, ctx.offset))
    ctx.columns = cjson.encode(dbLyrics.getLyricsByUserIdColumns())
    ctx.lyricsBaseLink = 'http://search.crintsoft.com/l/'

    utils.render_template("user/uploaded-lyrics.html", ctx)
end

return _M