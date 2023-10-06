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

class DatabaseApisHandler : public IHttpRequestHandler {
public:
    DatabaseApisHandler(sqlite3 *db, const string &uri);

    virtual const string &getUriPath() const override;
    virtual int onRequestHeader(HttpConnectionPtr connection) override;
    virtual int onRequestBody(HttpConnectionPtr connection) override;

protected:
    void prepareStmt(const string &sql, string &resultOut, string &messageOut, RapidjsonWriterX &writer);
    void queryStmt(const rapidjson::Document &body, string &resultOut, string &messageOut, RapidjsonWriterX &writer);

    struct Stmt {
        sqlite3_stmt                *stmt;
        string                      sql;
        VecStrings                  cols; // 无法预先获取到 column 的数据类型.
    };

    using MapStmts = map<string, Stmt>;

    string                          _uri;
    sqlite3                         *_db = nullptr;
    MapStmts                        _mapStmts;

};

#endif /* DatabaseApisHandler_hpp */
