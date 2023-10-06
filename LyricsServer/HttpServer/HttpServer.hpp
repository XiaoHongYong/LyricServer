//
//  HttpServer.hpp
//

#pragma once

#ifndef HTTP_SERVER_HPP
#define HTTP_SERVER_HPP

#include "../Types.h"
#include "uv.h"
#include "http_parser.h"

#include <set>
#include "HttpConnection.hpp"
#include "HttpRequest.hpp"
#include "IHttpRequestHandler.hpp"


class HttpServer {
public:
    HttpServer(const HttpServer &) = delete;
    HttpServer &operator=(const HttpServer &) = delete;

    HttpServer();

    int init(const string &address, int port);
    void quit();

    void run();

    IHttpRequestHandlerPtr getRequestHandler(const string &uri);
    void registerRequestHandler(IHttpRequestHandlerPtr handler) { _handlers.push_back(handler); }

    void startConnection(HttpConnectionPtr c);
    void stopConnection(HttpConnectionPtr connection);
    void stopAllConnections();

    uv_loop_t *loop() { return _loop; }

    virtual void dumpStatus(IJsonWriter *writer);

    static void connectionCallback(uv_stream_t *server, int status);

private:
    uv_loop_t                               *_loop = nullptr;
    uv_tcp_t                                _server;
    uv_signal_t                             _signal;

    std::set<HttpConnectionPtr>             _connections;

    std::vector<IHttpRequestHandlerPtr>     _handlers;
    IHttpRequestHandlerPtr                  _defaultHandler;

};


#endif  // HTTP_SERVER_HPP
