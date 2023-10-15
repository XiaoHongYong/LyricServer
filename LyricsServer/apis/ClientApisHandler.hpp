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

protected:
    LyricsServer                    *_server = nullptr;

};

#endif /* ClientApisHandler_hpp */
