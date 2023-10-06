//
//  StatusHandler.hpp
//

#pragma once

#ifndef StatusHandler_hpp
#define StatusHandler_hpp

#include "../HttpServer/IHttpRequestHandler.hpp"
#include "../LyricsServer.h"


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
