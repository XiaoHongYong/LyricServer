//
//  HttpRequest.hpp
//

#pragma once

#ifndef HttpRequest_h
#define HttpRequest_h

#include "../Types.h"


class HttpConnection;


struct HttpHeader {
    string      name;
    string      value;
};

enum HttpMethod {
    METHOD_INVALID              = 0,
    METHOD_GET                  = 1,
    METHOD_HEAD                 = 1 << 1,
    METHOD_POST                 = 1 << 2,
    METHOD_PUT                  = 1 << 3,
    METHOD_DELETE               = 1 << 4,
    METHOD_OPTIONS              = 1 << 5,
};

HttpMethod toHttpMethod(const string &method);

typedef std::list<HttpHeader>         ListHttpHeaders;

struct HttpRequest {
    HttpMethod                          methodID;
    string                              method;
    string                              uri;
    int                                 versionMajor;
    int                                 versionMinor;
    ListHttpHeaders                     headers;
    string                              body;
};

string *getHeaderByName(ListHttpHeaders &headers, const string &name);

#endif /* HttpRequest_h */
