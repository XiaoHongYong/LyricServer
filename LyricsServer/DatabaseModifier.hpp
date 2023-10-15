//
//  DatabaseModifier.hpp
//

#ifndef DatabaseModifier_hpp
#define DatabaseModifier_hpp

#include "Types.h"
#include "Sqlite3Utils.h"
#include "Utils/rapidjson.h"


struct DbApiCtx {
    rapidjson::Document             body;
    string                          result;
    string                          message;
};

/**
 * 为内部 Web server 提供服务
 * - 执行 insert/update/delete 三种修改操作.
 * - 记录操作日志，用于数据同步.
 */
class DatabaseModifier {
public:
    void init(sqlite3 *db, const string &tableName);

    bool executeAction(DbApiCtx &ctx, const string &action, const string &fields, const rapidjson::Value &args, int64_t &idReturned);

    const string &tableName() { return _tableName; }

protected:
    using MapStmts = map<string, sqlite3_stmt *>;

    sqlite3_stmt *getStmtByAction(DbApiCtx &ctx, const string &action, const string &fields);

    string                          _tableName;
    sqlite3                         *_db = nullptr;

    MapStmts                        _mapStmtsCreate;
    MapStmts                        _mapStmtsUpdate;
    MapStmts                        _mapStmtsDelete;

};

#endif /* DatabaseModifier_hpp */
