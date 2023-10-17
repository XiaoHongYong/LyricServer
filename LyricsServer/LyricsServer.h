#pragma once

#include "SpamLyricsFilter.h"
#include "LyricsDB.h"
#include "UserDB.h"
#include "HttpLib/HttpServer/HttpResponse.hpp"
#include "HttpLib/HttpServer/HttpServer.hpp"
#include "SyncRemoteMasterData.hpp"
#include "ServerConfig.hpp"


class LyricsServer : public HttpServer {
public:
    LyricsServer();
    virtual ~LyricsServer();

public:
    int init();

    int deleteLyricsFile(const string &relatedLink);
    void doDataSync(const string &filename);

    void dumpStatus(IJsonWriter *writer) override;

    // New uploaded lyrics.
    int addNewLyrics(LyricsInfo &lyrInfo, string &strLyrContent);

    // Update existing lyrics.
    int updateLyrics(LyricsInfo &lyrInfo, string &strLyrContent);

protected:
    bool executeDataSync(const StringView &line);

    int saveLyricsFile(LyricsInfo &lyrInfo, string &strLyrContentUtf8);

    void writeStatusLog();
    static void onTimerLogStatus(uv_timer_t *timer);

public:
    LyricsDB                    _dbLyrics;
    UserDB                      _dbUser;
    FilePtr                     _fpSyncLog;

    time_t                      _startTime;

    SyncRemoteMasterData        _syncRemoteMasterData;
    DatabaseModifier            _dbModifierUsers, _dbModifierLyrics;

    class ClientApisHandler     *_clientApisHandler = nullptr;

    uv_timer_t                  _timerLogStatus;
    StatusLog                   _statusLog;

    int64_t                     _latestCpuUsage = 0;

};
