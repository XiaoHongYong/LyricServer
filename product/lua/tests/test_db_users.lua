
local ut = require "tests.unittest"
local db_users = require "db_users"

local test_suite_db_users = {}

local t_username = '_user_abc_'
local t_email = '_email@test.com_'
local t_password = '_password_'

function test_suite_db_users.setup(ctx, conf)
    -- 先删除此账号
    local id = db_users.signin_user(t_email, t_password)
    if id ~= nil then
        ut.log('Account exits already, delete it first, id: ', id)
        local ret = db_users.remove_account(id)
        ut.ASSERT_NOT_NIL(ret)
    end

    ctx.username = t_username
    ctx.email = t_email
    ctx.password = t_password

    local ret = db_users.create_account(ctx)
    ut.ASSERT_TRUE(ret)

    ret = db_users.create_account(ctx)
    ut.ASSERT_FALSE(ret)
end

function test_suite_db_users.teardown(ctx, conf)
    -- 删除此账号
    local id = db_users.signin_user(t_email, t_password)
    ut.ASSERT_NOT_NIL(id)
    local ret = db_users.remove_account(id)
    ut.ASSERT_NOT_NIL(ret)
end

function test_suite_db_users.test_sign(ctx, conf)
    local id

    -- 登录失败
    id = db_users.signin_user(t_username, t_password .. 'x')
    ut.ASSERT_NIL(id)

    id = db_users.signin_user(t_email, t_password .. 'x')
    ut.ASSERT_NIL(id)

    -- 登录成功
    id = db_users.signin_user(t_username, t_password)
    ut.ASSERT_NOT_NIL(id)

    id = db_users.signin_user(t_email, t_password)
    ut.ASSERT_NOT_NIL(id)

    local user = db_users.get_user_by_id(id)
    ut.ASSERT_EQ(user.UserName, t_username)
    ut.ASSERT_EQ(user.Email, t_email)
end

function test_suite_db_users.test_change_email(ctx, conf)
    local ret

    -- 登录成功
    local id = db_users.signin_user(t_email, t_password)
    ut.ASSERT_NOT_NIL(id)

    local user = db_users.get_user_by_id(id)
    ut.ASSERT_NOT_NIL(user)
    ut.ASSERT_EQ(user.Email, t_email)

    user = db_users.get_user_by_email(t_email)
    ut.ASSERT_NOT_NIL(user)
    ut.ASSERT_EQ(user.id, id)

    local t_email_new = t_email .. '-new'

    ret = db_users.changeEmail(id, t_email_new)
    ut.ASSERT_EQ(ret, '')

    user = db_users.get_user_by_id(id)
    ut.ASSERT_EQ(user.Email, t_email_new)

    db_users.changeEmail(id, t_email)
end

function test_suite_db_users.test_change_password(ctx, conf)
    local ret

    -- 登录成功
    local id = db_users.signin_user(t_email, t_password)
    ut.ASSERT_NOT_NIL(id)

    local t_password_new = '-new-password-'

    ret = db_users.changePassword(id, t_password_new)
    ut.ASSERT_EQ(ret, '')

    id = db_users.signin_user(t_email, t_password_new)
    ut.ASSERT_NOT_NIL(id)

    id = db_users.signin_user(t_email, t_password)
    ut.ASSERT_NIL(id)

    id = db_users.signin_user(t_email, t_password_new)
    ut.ASSERT_NOT_NIL(id)

    ret = db_users.changePassword(id, t_password)
    ut.ASSERT_EQ(ret, '')
end

ut.add_test_suite('db_users', test_suite_db_users)

return _M 
