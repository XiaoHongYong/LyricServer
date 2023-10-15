//
//  StatusHandler.hpp
//

#pragma once

#ifndef StatusHandler_hpp
#define StatusHandler_hpp

#include "HttpLib/HttpServer/IHttpRequestHandler.hpp"
#include "../LyricsServer.h"


/**
 * 显示内部运行状态.
 */
class StatusHandler : public IHttpRequestHandler {
public:
    StatusHandler(LyricsServer *server);

    virtual const string &getUriPath() const override;
    virtual int onRequestHeader(HttpConnectionPtr connection) override;
    virtual int onRequestBody(HttpConnectionPtr connection) override;

protected:
    LyricsServer                    *_server = nullptr;

};

#endif /* StatusHandler_hpp */
