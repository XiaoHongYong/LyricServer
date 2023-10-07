#pragma once

#include "Sqlite3Utils.h"


#define        TIME_OFF_SET_FACTOR  100

class UserDB {
public:
    int init(const char *fileName);
    void Quit();

    int LoginUser(cstr_t szLoginName, cstr_t szUserPwdMask, long &nUserID);

    sqlite3 *db() { return m_db; }

protected:
    sqlite3                     *m_db = nullptr;
    sqlite3_stmt                *m_sqlLoginWithMLPassword = nullptr;

};
