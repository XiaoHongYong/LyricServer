#include "Types.h"
#include "LyricsServer.h"
#include "UserDB.h"


/**
 CREATE TABLE users (
         id integer primary key AUTOINCREMENT, MLPasswordHash text, PasswordHash text,
         Email text collate nocase, CreateDate date, LastLoginDate date,
         LastPasswordChangedDate date, UserName text collate nocase
     );

 CREATE UNIQUE INDEX users_UserName on users (UserName collate nocase);
 CREATE UNIQUE INDEX users_Email on users (Email collate nocase);

 */

#define SQL_LOGIN_WITH_MLPASSWORD   "SELECT id FROM users WHERE UserName=? and MLPasswordHash=?"

int UserDB::init(const char *fileName) {
    int ret = sqlite3_open(fileName, &m_db);
    if (ret != SQLITE_OK) {
        LOG(ERROR) << "Failed to open users db: " << fileName;
        return ERR_FALSE;
    }

    LOG(INFO) << "Open users database successfully: " << fileName;

    SQLIT3_STMT_PREPARE(m_db, SQL_LOGIN_WITH_MLPASSWORD, m_sqlLoginWithMLPassword);

    return ERR_OK;
}

void UserDB::Quit() {
    sqlite3_finalize(m_sqlLoginWithMLPassword);
}

int UserDB::LoginUser(cstr_t szLoginName, cstr_t szUserPwdMask, long &nUserID) {
    int ret = ERR_OK, n = 1;

    string name = toLower(szLoginName);

    sqlite3_reset(m_sqlLoginWithMLPassword);

    SQLITE3_BIND_TEXT(m_db, m_sqlLoginWithMLPassword, name.c_str());
    SQLITE3_BIND_TEXT(m_db, m_sqlLoginWithMLPassword, szUserPwdMask);

    // Check for password
    ret = sqlite3_step(m_sqlLoginWithMLPassword);
    if (ret == SQLITE_ERROR) {
        LOG(ERROR) << "Failed to sign in, error: " << sqlite3_errmsg(m_db);
        ret = ERR_DATABASE_ERROR;
    } else if (ret == SQLITE_ROW) {
        nUserID = sqlite3_column_int(m_sqlLoginWithMLPassword, 0);
        ret = ERR_OK;
    } else {
        ret = ERR_BAD_USERPWD;
        LOG(INFO) << "Failed to sign in, wrong username or password: " << szLoginName << ", " << szUserPwdMask;
    }

    sqlite3_reset(m_sqlLoginWithMLPassword);

    return ret;
}
