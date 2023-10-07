//
//  DatabaseApisHandler.cpp
//

#include "DatabaseApisHandler.hpp"
#include "../../../Utils/rapidjson.h"
#include "../RapidjsonWriter.hpp"
#include <rapidjson/reader.h>


cstr_t sqlite3ColumnTypeName(int type) {
    switch (type) {
        case SQLITE_INTEGER: return "integer";
        case SQLITE_FLOAT: return "float";
        case SQLITE_BLOB: return "blob";
        case SQLITE_NULL: return "null";
        case SQLITE_TEXT: return "text";
        default: return "unkown";
    }
}

void writeJsonFieldValue(IJsonWriter *writer, sqlite3_stmt *stmt, int colIdx) {
    sqlite3_value *value = sqlite3_column_value(stmt, colIdx);
    int type = sqlite3_value_type(value);
    switch (type) {
        case SQLITE_INTEGER: writer->writeInt64(sqlite3_value_int64(value)); break;
        case SQLITE_FLOAT: writer->writeDouble(sqlite3_value_double(value)); break;
        case SQLITE_BLOB: writer->writeString((char *)sqlite3_value_text(value)); break;
        case SQLITE_NULL: writer->writeNull(); break;
        case SQLITE_TEXT: writer->writeString((char *)sqlite3_value_text(value)); break;
        default: assert(0); writer->writeNull(); break;
    }
}

DatabaseApisHandler::DatabaseApisHandler(sqlite3 *db, const string &uri) : _db(db), _uri(uri) {
}

const string &DatabaseApisHandler::getUriPath() const {
    return _uri;
}

int DatabaseApisHandler::onRequestHeader(HttpConnectionPtr connection) {

    return ERR_OK;
}

int DatabaseApisHandler::onRequestBody(HttpConnectionPtr connection) {
    auto &bodyStr = connection->request().body;

    DbApiCtx ctx;
    RapidjsonWriterX writer;

    writer.startObject();

    rapidjson::Document &body = ctx.body;
    if (body.Parse(bodyStr.c_str(), bodyStr.size()).HasParseError() || !body.IsObject()) {
        ctx.result = "BAD-MESSAGE-FORMAT";
        ctx.message = "Post body should be json format.";
    } else {
        ctx.result = "OK";

        string action = getMemberString(body, "action");
        if (action == "prepare") {
            // Prepare statement
            prepareStmt(ctx, writer);
        } else if (action == "exec") {
            execSql(ctx, writer);
        } else if (action == "query") {
            queryStmt(ctx, writer);
        } else {
            ctx.result = "INVALID-ACTION";
            ctx.message = "Invalid action: " + action;
        }
    }

    writer.writePropString("result", ctx.result);
    if (!ctx.message.empty()) {
        writer.writePropString("message", ctx.message);
    }

    writer.endObject();

    auto &response = connection->response();
    response.statusCode = HttpStatusCode::OK;
    response.body.assign(writer.getString(), writer.getSize());
    response.sendAll();

    return ERR_OK;
}

void DatabaseApisHandler::prepareStmt(DbApiCtx &ctx, RapidjsonWriterX &writer) {
    sqlite3_stmt *stmt = nullptr;
    string sql = getMemberString(ctx.body, "sql");

    string stmtId = md5ToString(sql);
    auto it = _mapStmts.find(stmtId);
    if (it != _mapStmts.end()) {
        // 已经存在
        writer.writePropString("stmt-id", stmtId.c_str());

        // 返回字段名称
        writer.writePropStringArray("col-names", (*it).second.cols);
        return;
    }

    int ret = sqlite3_prepare(_db, sql.c_str(), -1, &stmt, NULL);
    if (ret != SQLITE_OK) {
        ctx.result = "SQL_PREPARE_FAILED";
        ctx.message = sqlite3_errmsg(_db);
        return;
    }

    Stmt item;
    item.stmt = stmt;
    item.sql = sql;

    int countCols = sqlite3_column_count(stmt);
    for (int i = 0; i < countCols; i++) {
        item.cols.push_back(sqlite3_column_name(stmt, i));
    }

    _mapStmts[stmtId] = item;

    writer.writePropString("stmt-id", stmtId.c_str());

    // 返回字段名称
    writer.writePropStringArray("col-names", item.cols);
}

void DatabaseApisHandler::queryStmt(DbApiCtx &ctx, RapidjsonWriterX &writer) {
    // execute statement
    auto digest = getMemberString(ctx.body, "stmt-id");
    auto it = _mapStmts.find(digest);
    if (it == _mapStmts.end()) {
        ctx.result = "STMT-NOT-EXISTS";
        ctx.message = "Can't find statement by id: " + digest;
        return;
    }

    queryStmt((*it).second, ctx, writer);
}

void DatabaseApisHandler::queryStmt(Stmt &stmt, DbApiCtx &ctx, RapidjsonWriterX &writer) {
    auto itArgs = ctx.body.FindMember("args");
    if (itArgs != ctx.body.MemberEnd()) {
        // 有参数
        auto &args = (*itArgs).value;
        if (!args.IsArray()) {
            ctx.result = "BAD-PARAMS";
            ctx.message = "args should be an array type.";
            return;
        }

        for (int i = 1; i <= args.Size(); i++) {
            // sqlite3_bind 的索引从 1 开始.
            auto &arg = args[i - 1];
            int ret = SQLITE_ERROR;
            switch (arg.GetType()) {
                case rapidjson::kNullType:
                    ret = sqlite3_bind_null(stmt.stmt, i);
                    break;
                case rapidjson::kFalseType:
                case rapidjson::kTrueType:
                    ret = sqlite3_bind_int(stmt.stmt, i, arg.GetBool());
                    break;
                case rapidjson::kObjectType:
                case rapidjson::kArrayType:
                    ctx.result = "BAD-PARAMS";
                    ctx.message = "Array and Object type is NOT supported in args.";
                    return;
                case rapidjson::kStringType:
                    ret = sqlite3_bind_text(stmt.stmt, i, arg.GetString(), arg.GetStringLength(), SQLITE_STATIC);
                    break;
                case rapidjson::kNumberType:
                    if (arg.IsInt()) {
                        ret = sqlite3_bind_int(stmt.stmt, i, arg.GetInt());
                    } else if (arg.IsDouble()) {
                        ret = sqlite3_bind_double(stmt.stmt, i, arg.GetDouble());
                    } else {
                        ret = sqlite3_bind_int64(stmt.stmt, i, arg.GetInt64());
                    }
                    break;
            }

            if (ret != SQLITE_OK) {
                ctx.result = "SQL-BIND-ERROR";
                ctx.message = stringPrintf("Failed to bind arg at index: %d. Error: %s", i, sqlite3_errmsg(_db));
                return;
            }
        }
    }

    if (getMemberBool(ctx.body, "col-names")) {
        // 返回字段名称
        writer.writePropStringArray("col-names", stmt.cols);
    }

    // 返回数据, rows 是二维数组[ [col1, col2], [col1, col2], ... ]
    writer.writeKey("rows");
    writer.startArray();
    while (true) {
        int ret = sqlite3_step(stmt.stmt);
        if (ret == SQLITE_ROW) {
            writer.startArray();
            for (int i = 0; i < stmt.cols.size(); i++) {
                writeJsonFieldValue(&writer, stmt.stmt, i);
            }
            writer.endArray();
        } else if (ret == SQLITE_DONE) {
            break;
        } else {
            ctx.result = "SQL_STEP_ERROR";
            ctx.message = sqlite3_errmsg(_db);
            break;
        }
    }
    writer.endArray();
}

void DatabaseApisHandler::execSql(DbApiCtx &ctx, RapidjsonWriterX &writer) {
    sqlite3_stmt *stmt = nullptr;
    string sql = getMemberString(ctx.body, "sql");

    int ret = sqlite3_prepare(_db, sql.c_str(), -1, &stmt, NULL);
    if (ret != SQLITE_OK) {
        ctx.result = "SQL_PREPARE_FAILED";
        ctx.message = sqlite3_errmsg(_db);
        return;
    }

    Stmt item;
    item.stmt = stmt;
    item.sql = sql;

    int countCols = sqlite3_column_count(stmt);
    for (int i = 0; i < countCols; i++) {
        item.cols.push_back(sqlite3_column_name(stmt, i));
    }

    queryStmt(item, ctx, writer);

    sqlite3_finalize(item.stmt);
}
