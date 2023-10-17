//
//  ClientApisHandler.hpp
//

#pragma once

#ifndef ClientApisHandler_hpp
#define ClientApisHandler_hpp

#include "HttpLib//HttpServer/IHttpRequestHandler.hpp"
#include "../LyricsServer.h"


/**
 * 给客户端应用提供 API 服务，包括歌词搜索、上传、用户登录
 */
class ClientApisHandler : public IHttpRequestHandler {
public:
    ClientApisHandler(LyricsServer *server);

    virtual const string &getUriPath() const override;
    virtual int onRequestHeader(HttpConnectionPtr connection) override;
    virtual int onRequestBody(HttpConnectionPtr connection) override;
    void dumpStatus(StatusLog &log) override;
    void dumpStatus(IJsonWriter *writer);

protected:
    int process(const string &data, HttpResponse &response);
    int processSearchCmd(MLMsgCmdSearch &cmdSearch, MLMsgRetSearch &retSearch);
    int processBatchSearchCmd(MLMsgCmdBatchSearch &cmdSearch, MLMsgRetBatchSearch &retSearch);
    int processUploadCmd(MLMsgCmdUpload &cmdUpload, MLMsgRetUpload &retMsg);

    int searchMatchedLyricsOnly(cstr_t szArCmp, cstr_t szTiCmp, RetLyrInfoList &listLyr);

    int searchBestMatchLyrics(cstr_t szArtist, cstr_t szAlbum, cstr_t szTitle, int nMediaLength, class MLLyricsInfoLite &infoLite);

protected:
    LyricsServer                    *_server = nullptr;

    LyricsDB                        &_dbLyrics;
    UserDB                          &_dbUser;

    SpamLyricsFilter                _spamFilter;
    CMLPacketWrapper                _packetWrapper;

    size_t                          _countTotal = 0, _countLogin = 0;
    size_t                          _countSearch = 0, _countSearchNotFound = 0, _countBatchSearch = 0;
    size_t                          _countUpload = 0, _countUploadExists = 0, _countUploadFailed = 0;
    size_t                          _countOldMessages = 0, _countBadMessages = 0;

};

#endif /* ClientApisHandler_hpp */
