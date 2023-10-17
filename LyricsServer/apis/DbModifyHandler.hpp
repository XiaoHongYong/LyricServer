//
//  DbModifyHandler.hpp
//

#pragma once

#ifndef DbModifyHandler_hpp
#define DbModifyHandler_hpp

#include "BaseJsonApiHandler.hpp"
#include "../DatabaseModifier.hpp"


using FunctionGetSql = std::function<string ()>;

/**
 * 为内部 Web server 提供服务
 * - 执行 insert/update/delete 三种修改操作.
 * - 记录操作日志，用于数据同步.
 */
class DbModifyHandler : public BaseJsonApiHandler {
public:
    DbModifyHandler(LyricsServer *server, sqlite3 *db, const string &tableName, const string &uri);

protected:
    void handleApi(DbApiCtx &ctx, RapidjsonWriterEx &writer) override;
    void dumpStatus(StatusLog &log) override;

    LyricsServer                    *_server;
    DatabaseModifier                _dbModifier;

    int64_t                         _countErrors = 0, _countRequests = 0;
};

#endif /* DbModifyHandler_hpp */
