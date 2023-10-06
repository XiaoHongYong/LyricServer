//
//  HttpRequestDefaultHandler.cpp
//

#include "HttpRequestDefaultHandler.hpp"


const string &HttpRequestDefaultHandler::getUriPath() const {
    static string path = "/";
    return path;
}

int HttpRequestDefaultHandler::onRequestHeader(HttpConnectionPtr connection) {
    HttpResponse &resp = connection->response();
    resp.statusCode = HttpStatusCode::NOT_FOUND;
    resp.body = "404 NOT FOUND";
    resp.sendAll();

    return ERR_OK;
}

int HttpRequestDefaultHandler::onRequestBody(HttpConnectionPtr connection) {
    HttpResponse &resp = connection->response();
    resp.statusCode = HttpStatusCode::NOT_FOUND;
    resp.body = "404 NOT FOUND";
    resp.sendAll();

    return ERR_OK;
}
