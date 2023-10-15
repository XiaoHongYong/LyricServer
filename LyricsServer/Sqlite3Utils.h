#pragma once

#include "../../third-parties/sqlite/sqlite3.h"


#define SQLIT3_STMT_PREPARE(db, SQL, sqlstmt)               \
    ret = sqlite3_prepare(db, SQL, -1, &sqlstmt, NULL);     \
    if (ret != SQLITE_OK) {                                 \
        LogSqlite3Error(db);                                \
        return ERR_FALSE;                                   \
    }


#define SQLITE3_BIND_TEXT(db, stmt, text)                       \
    ret = sqlite3_bind_text(stmt, n, text, -1, SQLITE_STATIC);  \
    if (ret != SQLITE_OK) {                                     \
        LogSqlite3Error(db);                                    \
        return ERR_FALSE;                                       \
    }                                                           \
    n++;

#define SQLITE3_BIND_INT(db, stmt, num)                         \
    ret = sqlite3_bind_int(stmt, n, num);                       \
    if (ret != SQLITE_OK) {                                     \
        LogSqlite3Error(db);                                    \
        return ERR_FALSE;                                       \
    }                                                           \
    n++;

#define SQLITE3_BIND_INT64(db, stmt, num)                         \
    ret = sqlite3_bind_int64(stmt, n, num);                       \
    if (ret != SQLITE_OK) {                                     \
        LogSqlite3Error(db);                                    \
        return ERR_FALSE;                                       \
    }                                                           \
    n++;


#define SQLITE3_GET_TEXT_FIELD(stmt, field)                     \
    text = (const char *)sqlite3_column_text(sqlStmt, n++);     \
    if (text) { field = text; }

#define SQLITE3_GET_INT_FIELD(stmt, field)                     \
    field = sqlite3_column_int(sqlStmt, n++);


inline void LogSqlite3Error(sqlite3 *db) {
    printf("Sqlite3 error: %s\n", sqlite3_errmsg(db));
}
