//
//  HttpRequestDefaultHandler.hpp
//

#ifndef HttpRequestDefaultHandler_hpp
#define HttpRequestDefaultHandler_hpp

#include "IHttpRequestHandler.hpp"


class HttpRequestDefaultHandler : public IHttpRequestHandler {
public:
    virtual const string &getUriPath() const override;
    virtual int onRequestHeader(HttpConnectionPtr connection) override;
    virtual int onRequestBody(HttpConnectionPtr connection) override;

};

#endif /* HttpRequestDefaultHandler_hpp */
