//
//  BaseEncryptApiHandler.hpp
//

#pragma once

#ifndef BaseEncryptApiHandler_hpp
#define BaseEncryptApiHandler_hpp

#include "BaseJsonApiHandler.hpp"
#include "aes.hpp"


/**
 * 定义加密 API 的基本接口
 */
class BaseEncryptApiHandler : public IHttpRequestHandler {
public:
    BaseEncryptApiHandler(const string &uri);

    virtual const string &getUriPath() const override;
    virtual int onRequestHeader(HttpConnectionPtr connection) override;
    virtual int onRequestBody(HttpConnectionPtr connection) override;

protected:
    virtual void handleApi(DbApiCtx &ctx, RapidjsonWriterEx &writer) = 0;

    string                          _uri;
    AES_ctx                         _aesCtx;

};

#endif /* BaseEncryptApiHandler_hpp */
