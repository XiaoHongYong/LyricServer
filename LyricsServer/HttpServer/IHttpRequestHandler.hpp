//
//  IHttpRequestHandler.hpp
//

#ifndef IHttpRequestHandler_hpp
#define IHttpRequestHandler_hpp

#include "HttpConnection.hpp"


class IHttpRequestHandler {
public:
    virtual ~IHttpRequestHandler() {}

    virtual const string &getUriPath() const = 0;
    virtual int onRequestHeader(HttpConnectionPtr connection) = 0;
    virtual int onRequestBody(HttpConnectionPtr connection) = 0;

};

typedef std::shared_ptr<IHttpRequestHandler>    IHttpRequestHandlerPtr;


#endif /* IHttpRequestHandler_hpp */
