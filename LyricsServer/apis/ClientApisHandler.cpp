//
//  ClientApisHandler.cpp
//

#include "ClientApisHandler.hpp"


ClientApisHandler::ClientApisHandler(LyricsServer *server) : _server(server) {
}

const string &ClientApisHandler::getUriPath() const {
    static string path = "/searchlyrics.htm";
    return path;
}

int ClientApisHandler::onRequestHeader(HttpConnectionPtr connection) {

    return ERR_OK;
}

int ClientApisHandler::onRequestBody(HttpConnectionPtr connection) {
    auto &body = connection->request().body;
    _server->process((uint8_t *)body.c_str(), body.size(), connection->response());

    return ERR_OK;
}
