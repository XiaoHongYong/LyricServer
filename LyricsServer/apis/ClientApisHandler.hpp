//
//  ClientApisHandler.hpp
//

#pragma once

#ifndef ClientApisHandler_hpp
#define ClientApisHandler_hpp

#include "../HttpServer/IHttpRequestHandler.hpp"
#include "../LyricsServer.h"


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
