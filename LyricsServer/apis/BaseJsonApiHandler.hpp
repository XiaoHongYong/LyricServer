//
//  BaseJsonApiHandler.hpp
//

#pragma once

#ifndef BaseJsonApiHandler_hpp
#define BaseJsonApiHandler_hpp

#include "HttpLib/HttpServer/IHttpRequestHandler.hpp"
#include "../LyricsServer.h"
#include "../DatabaseModifier.hpp"


/**
 * 定义 JSON API 的基本接口
 */
class BaseJsonApiHandler : public IHttpRequestHandler {
public:
    BaseJsonApiHandler(const string &uri);

    virtual const string &getUriPath() const override;
    virtual int onRequestHeader(HttpConnectionPtr connection) override;
    virtual int onRequestBody(HttpConnectionPtr connection) override;

protected:
    virtual void handleApi(DbApiCtx &ctx, RapidjsonWriterEx &writer) = 0;

    string                          _uri;

};

#endif /* BaseJsonApiHandler_hpp */
