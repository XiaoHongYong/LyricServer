//
//  HttpServer.cpp
//

#include "HttpServer.hpp"
#include "HttpRequestDefaultHandler.hpp"
#include <glog/logging.h>
#include <signal.h>
#include <utility>


const int BACKLOG = 1024;

void on_uv_walk(uv_handle_t* handle, void* arg) {
    uv_close(handle, NULL);
}

void on_sigint_received(uv_signal_t *handle, int signum) {
    int result = uv_loop_close(handle->loop);
    if (result == UV_EBUSY) {
        uv_walk(handle->loop, on_uv_walk, NULL);
    }
}

void HttpServer::connectionCallback(uv_stream_t *server, int status) {
    assert(status == 0);
    HttpServer *httpServer = (HttpServer *)server->data;

    HttpConnectionPtr client = std::make_shared<HttpConnection>(*httpServer);

    int r = uv_tcp_init(server->loop, client->uv_tcp_handle());
    assert(r == 0);

    r = uv_accept(server, client->uv_stream_handle());
    if (r) {
        client->stop();
    } else {
        httpServer->startConnection(client);
    }
}

HttpServer::HttpServer() {
    memset(&_server, 0, sizeof(_server));

    _server.data = this;
}

int HttpServer::init(const string &address, int port) {
    initHttpReqParserSettings();

    uv_signal_init(uv_default_loop(), &_signal);
    uv_signal_start(&_signal, on_sigint_received, SIGINT);
    uv_signal_start(&_signal, on_sigint_received, SIGTERM);
#if defined(SIGQUIT)
    uv_signal_start(&_signal, on_sigint_received, SIGQUIT);
#endif  // defined(SIGQUIT)

    _loop = uv_default_loop();

    int r = uv_tcp_init(_loop, &_server);
    assert(r == 0);

    struct sockaddr_in addr;
    r = uv_ip4_addr(address.c_str(), port, &addr);
    assert(r == 0);

    r = uv_tcp_bind(&_server, (struct sockaddr *)&addr, 0);
    assert(r == 0);

    r = uv_listen((uv_stream_t *)&_server, BACKLOG, connectionCallback);
    assert(r == 0);

    _defaultHandler = std::make_shared<HttpRequestDefaultHandler>();

    return ERR_OK;
}

void HttpServer::run() {
    int r = uv_run(_loop, UV_RUN_DEFAULT);
    assert(r == 0);

    uv_loop_close(uv_default_loop());
}

void HttpServer::quit() {
    uv_loop_close(_loop);
}

IHttpRequestHandlerPtr HttpServer::getRequestHandler(const string &uri) {
    for (auto handler : _handlers) {
        if (startsWith(uri.c_str(), handler->getUriPath().c_str())) {
            return handler;
        }
    }

    return _defaultHandler;
}

void HttpServer::startConnection(HttpConnectionPtr c) {
    _connections.insert(c);
    c->start();
}

void HttpServer::stopConnection(HttpConnectionPtr c) {
    _connections.erase(c);
    c->stop();
}

void HttpServer::stopAllConnections() {
    for (auto c : _connections) {
        c->stop();
    }

    _connections.clear();
}

void HttpServer::dumpStatus(IJsonWriter *writer) {
    writer->startArray();

    for (auto &c : _connections) {
        c->dumpStatus(writer);
    }

    writer->endArray();
}
