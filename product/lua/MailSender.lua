local mail = require "resty.mail"
local conf = require 'conf'

local _M = {}
function _M.sendMail(toAddress, fromAddress, subject, text)
    local mailer, err = mail.new({
        host = "email-smtp.us-west-2.amazonaws.com",
        port = 587,
        starttls = true,
        username = conf.smtp_username,
        password = conf.smtp_password,
    })

    if err then
        ngx.log(ngx.ERR, "mail.new error: ", err)
        return false
    end

    local ok, err = mailer:send({
        from = fromAddress,
        to = { toAddress },
        subject = subject,
        text = text,
        -- html = text,
    })
    if err then
        ngx.log(ngx.ERR, "mailer:send error: ", err)
        return false
    end

    return true
end

return _M