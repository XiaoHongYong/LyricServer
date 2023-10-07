//
//  DatabaseApisHandler.hpp
//

#pragma once

#ifndef DatabaseApisHandler_hpp
#define DatabaseApisHandler_hpp

#include "../HttpServer/IHttpRequestHandler.hpp"
#include "../LyricsServer.h"
#include "../RapidjsonWriter.hpp"
#include <rapidjson/document.h>


using FuncionWriteJsonField = void (*)(IJsonWriter *writer, sqlite3_stmt *stmt, int colIdx);

struct DbApiCtx {
    rapidjson::Document             body;
    string                          result;
    string                          message;
};

class DatabaseApisHandler : public IHttpRequestHandler {
public:
    DatabaseApisHandler(sqlite3 *db, const string &uri);

    virtual const string &getUriPath() const override;
    virtual int onRequestHeader(HttpConnectionPtr connection) override;
    virtual int onRequestBody(HttpConnectionPtr connection) override;

protected:
    struct Stmt {
        sqlite3_stmt                *stmt;
        string                      sql;
        VecStrings                  cols; // 无法预先获取到 column 的数据类型.
    };

    using MapStmts = map<string, Stmt>;

    void prepareStmt(DbApiCtx &ctx, RapidjsonWriterX &writer);
    void querySql(DbApiCtx &ctx, RapidjsonWriterX &writer);
    void queryStmt(DbApiCtx &ctx, RapidjsonWriterX &writer);
    void queryStmt(Stmt &stmt, DbApiCtx &ctx, RapidjsonWriterX &writer);
    void execSql(DbApiCtx &ctx, RapidjsonWriterX &writer);

    string                          _uri;
    sqlite3                         *_db = nullptr;
    MapStmts                        _mapStmts;

};

#endif /* DatabaseApisHandler_hpp */
