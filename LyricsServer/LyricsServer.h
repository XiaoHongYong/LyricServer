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
    void process(uint8_t *data, size_t len, HttpResponse &response);

    int deleteLyricsFile(const string &relatedLink);
    void doDataSync(const string &filename);

protected:
    bool executeDataSync(const StringView &line);

    int processSearchCmd(MLMsgCmdSearch &cmdSearch, MLMsgRetSearch &retSearch);
    int processBatchSearchCmd(MLMsgCmdBatchSearch &cmdSearch, MLMsgRetBatchSearch &retSearch);
    int processUploadCmd(MLMsgCmdUpload &cmdUpload, MLMsgRetUpload &retMsg);

    int searchMatchedLyricsOnly(cstr_t szArCmp, cstr_t szTiCmp, RetLyrInfoList &listLyr);

    int searchBestMatchLyrics(cstr_t szArtist, cstr_t szAlbum, cstr_t szTitle, int nMediaLength, class MLLyricsInfoLite &infoLite);

    // New uploaded lyrics.
    int addNewLyrics(LyricsInfo &lyrInfo, string &strLyrContent);

    // Update existing lyrics.
    int updateLyrics(LyricsInfo &lyrInfo, string &strLyrContent);

    int saveLyricsFile(LyricsInfo &lyrInfo, string &strLyrContentUtf8);

    /*
    static void databaseUpdateHook(void *dataUser, int type, const char *dbName, const char *tableName, sqlite3_int64 rowId);

    struct DbUpdateHookCtx {
        bool                    logDeleteOnly = false;
        sqlite3                 *db = nullptr;
        LyricsServer            *thiz = nullptr;
        string                  tableName;
        sqlite3_stmt            *stmt = nullptr;
        VecStrings              cols;
    };
protected:
     DbUpdateHookCtx             _userDbHookCtx, _lyricsDbHookCtx;
     */

public:
    CMLPacketWrapper            m_packetWrapper;

    string                      m_lyricsDir;
    string                      m_relatedUploadDir;
    string                      m_lyricsHttpLinkBase;

    SpamLyricsFilter            m_spamFilter;
    LyricsDB                    m_dbLyrics;
    UserDB                      m_dbUser;
    FilePtr                     m_fpSyncLog;

    time_t                      m_startTime;
    size_t                      m_countSearch = 0, m_countSearchNotFound = 0;
    size_t                      m_countUpload = 0, m_countUploadExists = 0, m_countUploadFailed = 0;
    bool                        m_isMaster = true;

    SyncRemoteMasterData        m_syncRemoteMasterData;
    DatabaseModifier            _dbModifierUsers, _dbModifierLyrics;

};
