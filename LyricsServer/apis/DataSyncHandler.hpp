//
//  DataSyncHandler.hpp
//

#pragma once

#ifndef DataSyncHandler_hpp
#define DataSyncHandler_hpp

#include "BaseEncryptApiHandler.hpp"


/**
 * 为内部 Web server 提供服务
 * - 执行 insert/update/delete 三种修改操作.
 * - 记录操作日志，用于数据同步.
 */
class DataSyncHandler : public BaseEncryptApiHandler {
public:
    DataSyncHandler(LyricsServer *server);

protected:
    void handleApi(DbApiCtx &ctx, RapidjsonWriterEx &writer) override;

    LyricsServer                    *_server;

};

#endif /* DataSyncHandler_hpp */
