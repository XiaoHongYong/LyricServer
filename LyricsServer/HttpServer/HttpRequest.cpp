//
//  HttpRequest.cpp
//

#include "HttpRequest.hpp"


HttpMethod toHttpMethod(const string &method) {
    if (method == "GET") {
        return METHOD_GET;
    } else if (method == "POST") {
        return METHOD_POST;
    } else if (method == "PUT") {
        return METHOD_PUT;
    } else if (method == "DELETE") {
        return METHOD_DELETE;
    } else if (method == "OPTIONS") {
        return METHOD_OPTIONS;
    }

    return METHOD_INVALID;
}

string *getHeaderByName(ListHttpHeaders &headers, const string &name) {
    for (auto &header : headers) {
        if (header.name == name) {
            return &header.value;
        }
    }

    return nullptr;
}
