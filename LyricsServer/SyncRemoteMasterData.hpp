//
//  SyncRemoteMasterData.hpp
//

#ifndef SyncRemoteMasterData_hpp
#define SyncRemoteMasterData_hpp

#include <rapidjson/document.h>
#include "HttpLib/HttpClient.hpp"
#include "aes.hpp"
#include "DatabaseModifier.hpp"


class LyricsServer;

/**
 * SyncRemoteMasterData 负责从 master 同步 data-sync-log
 */
class SyncRemoteMasterData {
public:
    void start(LyricsServer *server);

protected:
    static void onTimer(uv_timer_t *timer);

    void sendSyncRequest();
    void onSync(rapidjson::Value &data);

    LyricsServer            *_server = nullptr;
    uv_timer_t              _timer;
    HttpClient              _client;
    AES_ctx                 _aesCtx;

};

#endif /* SyncRemoteMasterData_hpp */
