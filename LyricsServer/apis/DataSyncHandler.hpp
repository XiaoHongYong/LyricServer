//
//  DataSyncHandler.hpp
//

#pragma once

#ifndef DataSyncHandler_hpp
#define DataSyncHandler_hpp

#include "BaseEncryptApiHandler.hpp"


/**
 * DataSyncHandler 为 slave 节点提供数据同步 API 服务
 * - 响应 SyncRemoteMasterData 的请求，将写入的 data-sync-log 加密，然后发送给回去
 */
class DataSyncHandler : public BaseEncryptApiHandler {
public:
    DataSyncHandler(LyricsServer *server);

protected:
    void handleApi(DbApiCtx &ctx, RapidjsonWriterEx &writer) override;
    void dumpStatus(StatusLog &log) override;

    LyricsServer                    *_server;
    int                             _countRequests = 0, _countErrors = 0;

};

#endif /* DataSyncHandler_hpp */
