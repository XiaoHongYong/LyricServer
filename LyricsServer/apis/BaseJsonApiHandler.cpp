//
//  BaseJsonApiHandler.cpp
//

#include <rapidjson/reader.h>
#include "BaseJsonApiHandler.hpp"
#include "Utils/rapidjson.h"


BaseJsonApiHandler::BaseJsonApiHandler(const string &uri) : _uri(uri) {
}

const string &BaseJsonApiHandler::getUriPath() const {
    return _uri;
}

int BaseJsonApiHandler::onRequestHeader(HttpConnectionPtr connection) {
    if (connection->request().methodID != HttpMethod::METHOD_POST) {
        auto &response = connection->response();
        response.statusCode = HttpStatusCode::NOT_FOUND;
        response.body = "404 NOT-FOUND-JSON";
        response.sendAll();
    }
    return ERR_OK;
}

int BaseJsonApiHandler::onRequestBody(HttpConnectionPtr connection) {
    auto &bodyStr = connection->request().body;

    DbApiCtx ctx;
    RapidjsonWriterEx writer;

    writer.startObject();

    rapidjson::Document &body = ctx.body;
    if (body.Parse(bodyStr.c_str(), bodyStr.size()).HasParseError() || !body.IsObject()) {
        ctx.result = "BAD-MESSAGE-FORMAT";
        ctx.message = "Post body should be json format.";
        LOG(INFO) << "";
    } else {
        ctx.result = "OK";
        handleApi(ctx, writer);
    }

    writer.writePropString("result", ctx.result);
    if (!ctx.message.empty()) {
        writer.writePropString("message", ctx.message);
    }

    writer.endObject();

    auto &response = connection->response();
    response.statusCode = HttpStatusCode::OK;
    response.body.assign(writer.getString(), writer.getSize());
    response.sendAll();

    return ERR_OK;
}
