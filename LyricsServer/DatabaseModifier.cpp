//
//  DatabaseModifier.cpp
//

#include "DatabaseModifier.hpp"
#include "apis/DatabaseApisHandler.hpp"


string getDeleteSql(const string &tableName, const string &fields) {
    return stringPrintf("DELETE FROM %s WHERE id = ?", tableName.c_str());
}

string getInsertSql(const string &tableName, const string &fields) {
    VecStrings vFields;
    StringView(fields).split(',', vFields);

    string values("?");
    for (int i = 1; i < vFields.size(); i++) {
        values.append(",?");
    }

    return stringPrintf("INSERT INTO %s (%s) VALUES (%s) RETURNING id", tableName.c_str(), fields.c_str(), values.c_str());
}

string getUpdateSql(const string &tableName, const string &fields) {
    VecStrings vFields;
    StringView(fields).split(',', vFields);

    string values;
    for (int i = 0; i < vFields.size(); i++) {
        if (!values.empty()) {
            values.append(",");
        }
        values.append(vFields[i] + "=?");
    }

    return stringPrintf("UPDATE %s set %s WHERE id=?", tableName.c_str(), values.c_str());
}

void DatabaseModifier::init(sqlite3 *db, const string &tableName) {
    _db = db;
    _tableName = tableName;
}

sqlite3_stmt *DatabaseModifier::getStmtByAction(DbApiCtx &ctx, const string &action, const string &fields) {
    MapStmts *mapStmts = nullptr;
    std::function<string(const string &, const string &)> funcGetSql;

    if (action == "delete") {
        mapStmts = &_mapStmtsDelete;
        funcGetSql = getDeleteSql;
    } else if (action == "create") {
        mapStmts = &_mapStmtsCreate;
        funcGetSql = getInsertSql;
    } else if (action == "update") {
        mapStmts = &_mapStmtsUpdate;
        funcGetSql = getUpdateSql;
    } else {
        ctx.result = "INVALID-ACTION";
        ctx.message = "Invalid action: " + action;
        LOG(INFO) << "Invalid action:" << action;
        return nullptr;
    }

    sqlite3_stmt *stmt = nullptr;
    auto it = mapStmts->find(fields);
    if (it != mapStmts->end()) {
        stmt = (*it).second;
    } else {
        string sql = funcGetSql(_tableName, fields);

        int ret = sqlite3_prepare(_db, sql.c_str(), -1, &stmt, NULL);
        if (ret != SQLITE_OK) {
            ctx.result = "SQL_PREPARE_FAILED";
            ctx.message = sqlite3_errmsg(_db);
            LOG(INFO) << "Prepare statement failed: " << sql << ", error: " << sqlite3_errmsg(_db);
            return nullptr;
        }

        (*mapStmts)[fields] = stmt;
    }

    return stmt;
}

bool DatabaseModifier::executeAction(DbApiCtx &ctx, const string &action, const string &fields, const rapidjson::Value &args, int64_t &idReturned) {
    sqlite3_stmt *stmt = getStmtByAction(ctx, action, fields);
    if (stmt == nullptr) {
        return false;
    }

    sqlite3_reset(stmt);

    if (!bindStmtArgs(ctx, _db, stmt, args)) {
        return false;
    }

    idReturned = -1;
    int ret = sqlite3_step(stmt);
    if (ret == SQLITE_ROW) {
        sqlite3_value *value = sqlite3_column_value(stmt, 0);
        int type = sqlite3_value_type(value);
        if (type == SQLITE_INTEGER) {
            idReturned = sqlite3_value_int64(value);
        } else {
            return false;
        }
    } else if (ret == SQLITE_DONE) {
        idReturned = -1;
    } else {
        ctx.result = "SQL_STEP_ERROR";
        ctx.message = sqlite3_errmsg(_db);
        LOG(ERROR) << "sqlite3_step failed: " << sqlite3_errmsg(_db);
        return false;
    }

    sqlite3_reset(stmt);

    return true;
}
