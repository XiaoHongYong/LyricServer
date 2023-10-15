local mail = require "resty.mail"

local _M = {}
function _M.sendMail(toAddress, fromAddress, subject, text)
    local mailer, err = mail.new({
        host = "email-smtp.us-west-2.amazonaws.com",
        port = 587,
        starttls = true,
        username = "AKIA3Y6DGYOKWJTDRU4N",
        password = "BOJk/LrlVyZXT667gjWpHYgseKmKpfKq9/3+qVqI7LsK",
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