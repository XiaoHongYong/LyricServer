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
/*
void writeSqliteJsonFieldInt(IJsonWriter *writer, sqlite3_stmt *stmt, int colIdx) {
    sqlite3_value *value = sqlite3_column_value(stmt, colIdx);
    if (sqlite3_value_type(value) == SQLITE_NULL) {
        writer->writeNull();
    } else {
        writer->writeInt64(sqlite3_value_int64(value));
    }
}

void writeSqliteJsonFieldString(IJsonWriter *writer, sqlite3_stmt *stmt, int colIdx) {
    sqlite3_value *value = sqlite3_column_value(stmt, colIdx);
    if (sqlite3_value_type(value) == SQLITE_NULL) {
        writer->writeNull();
    } else {
        writer->writeString((char *)sqlite3_value_text(value));
    }
}

void writeSqliteJsonFieldFloat(IJsonWriter *writer, sqlite3_stmt *stmt, int colIdx) {
    sqlite3_value *value = sqlite3_column_value(stmt, colIdx);
    if (sqlite3_value_type(value) == SQLITE_NULL) {
        writer->writeNull();
    } else {
        writer->writeDouble(sqlite3_value_double(value));
    }
}

void writeSqliteJsonFieldNull(IJsonWriter *writer, sqlite3_stmt *stmt, int colIdx) {
    writer->writeNull();
}

FuncionWriteJsonField sqlite3FieldTypeToJsonFieldFunction(int type) {
    switch (type) {
        case SQLITE_INTEGER: return writeSqliteJsonFieldInt;
        case SQLITE_FLOAT: return writeSqliteJsonFieldFloat;
        case SQLITE_BLOB: return writeSqliteJsonFieldString;
        case SQLITE_NULL: return writeSqliteJsonFieldNull;
        case SQLITE_TEXT: return writeSqliteJsonFieldString;
        default: assert(0); return writeSqliteJsonFieldNull;
    }
}*/

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

    rapidjson::Document body;
    if (body.Parse(bodyStr.c_str(), bodyStr.size()).HasParseError()) {
        return ERR_PARSE_JSON;
    }

    assert(body.IsObject());
    if (!body.IsObject()) {
        return ERR_BAD_MSG;
    }

    string action = getMemberString(body, "action");

    RapidjsonWriterX writer;

    writer.startObject();

    string result = "OK", message;

    if (action == "prepare") {
        // Prepare statement
        string sql = getMemberString(body, "sql");

        prepareStmt(sql, result, message, writer);
    } else if (action == "exec") {
        // Prepare statement
    } else if (action == "query") {
        queryStmt(body, result, message, writer);
    } else {
        result = "INVALID-ACTION";
        message = "Invalid action: " + action;
    }

    writer.writePropString("result", result);
    if (!message.empty()) {
        writer.writePropString("message", message);
    }

    writer.endObject();

    auto &response = connection->response();
    response.statusCode = HttpStatusCode::OK;
    response.body.assign(writer.getString(), writer.getSize());
    response.sendAll();

    return ERR_OK;
}

void DatabaseApisHandler::prepareStmt(const string &sql, string &resultOut, string &messageOut, RapidjsonWriterX &writer) {
    sqlite3_stmt *stmt = nullptr;

    int ret = sqlite3_prepare(_db, sql.c_str(), -1, &stmt, NULL);
    if (ret != SQLITE_OK) {
        resultOut = "SQL_PREPARE_FAILED";
        messageOut = sqlite3_errmsg(_db);
        return;
    }

    Stmt item;
    item.stmt = stmt;
    item.sql = sql;

    int countCols = sqlite3_column_count(stmt);
    for (int i = 0; i < countCols; i++) {
        item.cols.push_back(sqlite3_column_name(stmt, i));
    }

    string stmtId = md5ToString(sql);
    _mapStmts[stmtId] = item;

    writer.writePropString("stmt-id", stmtId.c_str());
}

void DatabaseApisHandler::queryStmt(const rapidjson::Document &body, string &resultOut, string &messageOut, RapidjsonWriterX &writer) {
    // execute statement
    auto digest = getMemberString(body, "stmt-id");
    auto it = _mapStmts.find(digest);
    if (it == _mapStmts.end()) {
        resultOut = "STMT-NOT-EXISTS";
        messageOut = "Can't find statement by id: " + digest;
        return;
    }
    auto &stmt = (*it).second;

    auto itArgs = body.FindMember("args");
    if (itArgs != body.MemberEnd()) {
        // 有参数
        auto &args = (*itArgs).value;
        if (!args.IsArray()) {
            resultOut = "BAD-PARAMS";
            messageOut = "args should be an array type.";
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
                    resultOut = "BAD-PARAMS";
                    messageOut = "Array and Object type is NOT supported in args.";
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
                resultOut = "SQL-BIND-ERROR";
                messageOut = stringPrintf("Failed to bind arg at index: %d. Error: %s", i, sqlite3_errmsg(_db));
                return;
            }
        }

        if (getMemberBool(body, "col-names")) {
            // 返回字段名称
            writer.writeKey("col-names");

            writer.startArray();
            for (int i = 0; i < stmt.cols.size(); i++) {
                writer.writeString(stmt.cols[i].c_str());
            }
            writer.endArray();
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
                resultOut = "SQL_STEP_ERROR";
                messageOut = sqlite3_errmsg(_db);
                break;
            }
        }
        writer.endArray();
    }
}
