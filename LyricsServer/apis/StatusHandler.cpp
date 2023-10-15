//
//  StatusHandler.cpp
//

#include "StatusHandler.hpp"
#include "Utils/rapidjson.h"
#include <rapidjson/prettywriter.h>


StatusHandler::StatusHandler(LyricsServer *server) : _server(server) {
}

const string &StatusHandler::getUriPath() const {
    static string path = "/status/";
    return path;
}

int StatusHandler::onRequestHeader(HttpConnectionPtr connection) {
    RapidjsonPrettyWriterEx writer;
    _server->dumpStatus(&writer);

    auto &response = connection->response();
    response.body = writer.getString();
    response.statusCode = HttpStatusCode::OK;
    response.sendAll();

    return ERR_OK;
}

int StatusHandler::onRequestBody(HttpConnectionPtr connection) {
    return ERR_OK;
}
