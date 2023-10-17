//
//  DbModifyHandler.cpp
//

#include <rapidjson/reader.h>
#include "DbModifyHandler.hpp"
#include "DatabaseApisHandler.hpp"
#include "Utils/rapidjson.h"
#include "../DataSyncLog.hpp"


DbModifyHandler::DbModifyHandler(LyricsServer *server, sqlite3 *db, const string &tableName, const string &uri) : BaseJsonApiHandler(uri), _server(server) {
    _dbModifier.init(db, tableName);
}

void DbModifyHandler::handleApi(DbApiCtx &ctx, RapidjsonWriterEx &writer) {
    _countRequests++;

    string action = getMemberString(ctx.body, "action");
    string fields = getMemberString(ctx.body, "fields");

    auto itArgs = ctx.body.FindMember("args");
    if (itArgs == ctx.body.MemberEnd() || !(*itArgs).value.IsArray()) {
        ctx.result = "BAD-PARAMS";
        ctx.message = "args should be an array type.";
        LOG(ERROR) << "DbModifyHandler::handleApi, incorrect params 'args'" << action << ", fields: " << fields;
        _countErrors++;
        return;
    }

    LOG(INFO) << "DbModifyHandler::handleApi, action:" << action << ", fields: " <<
        fields << ", args";

    auto &args = (*itArgs).value;
    int64_t idReturned = -1;
    if (_dbModifier.executeAction(ctx, action, fields, args, idReturned)) {
        writer.writePropInt64("id", idReturned);

        // 写入日志
        if (idReturned != -1) {
            fields.append(",id");
            args.PushBack(idReturned, ctx.body.GetAllocator());
        }

        dslWriteDbAction(_server->_fpSyncLog, _dbModifier.tableName(), action, fields, args);
    }
}

void DbModifyHandler::dumpStatus(StatusLog &log) {
    log.writeInt("db-api-errors", _countErrors);
    log.writeInt("db-api-request", _countRequests);

    _countErrors = 0;
    _countRequests = 0;
}
