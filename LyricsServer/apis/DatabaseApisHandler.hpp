//
//  DatabaseApisHandler.hpp
//

#pragma once

#ifndef DatabaseApisHandler_hpp
#define DatabaseApisHandler_hpp

#include "BaseJsonApiHandler.hpp"


void writeJsonFieldValue(IJsonWriter *writer, sqlite3_stmt *stmt, int colIdx);
bool bindStmtArgs(DbApiCtx &ctx, sqlite3 *db, sqlite3_stmt *stmt, const rapidjson::Value &args);

/**
 * 为内部 Web server 提供服务
 * - 执行 sql 查询，不要执行数据修改
 */
class DatabaseApisHandler : public BaseJsonApiHandler {
public:
    DatabaseApisHandler(sqlite3 *db, const string &uri);

protected:
    struct Stmt {
        sqlite3_stmt                *stmt;
        string                      sql;
        VecStrings                  cols; // 无法预先获取到 column 的数据类型.
    };

    using MapStmts = map<string, Stmt>;

    void handleApi(DbApiCtx &ctx, RapidjsonWriterEx &writer) override;
    void dumpStatus(StatusLog &log) override;

    void prepareStmt(DbApiCtx &ctx, RapidjsonWriterEx &writer);
    void querySql(DbApiCtx &ctx, RapidjsonWriterEx &writer);
    void queryStmt(DbApiCtx &ctx, RapidjsonWriterEx &writer);
    void queryStmt(Stmt &stmt, DbApiCtx &ctx, RapidjsonWriterEx &writer);
    void execSql(DbApiCtx &ctx, RapidjsonWriterEx &writer);

    sqlite3                         *_db = nullptr;
    MapStmts                        _mapStmts;

    int64_t                         _countErrors = 0, _countRequests = 0;

};

#endif /* DatabaseApisHandler_hpp */
