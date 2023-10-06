#pragma once

#include "SpamLyricsFilter.h"
#include "LyricsDB.h"
#include "UserDB.h"
#include "HttpServer/HttpResponse.hpp"
#include "HttpServer/HttpServer.hpp"


struct ServerConfig {
    string              rootDir;
    string              lyricsDir;
    string              uploadDirName; // 相对于 lyricsDir 的目录名
    string              lyricsHttpUrlBase;

    string              address;
    int                 port;
};

class LyricsServer : public HttpServer {
public:
    LyricsServer();
    virtual ~LyricsServer();

public:
    int init(ServerConfig &config);
    void process(uint8_t *data, size_t len, HttpResponse &response);

protected:
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

};
