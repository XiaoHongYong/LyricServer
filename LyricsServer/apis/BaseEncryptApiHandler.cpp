//
//  BaseEncryptApiHandler.cpp
//

#include <rapidjson/reader.h>
#include "BaseEncryptApiHandler.hpp"
#include "Utils/rapidjson.h"
#include "AesUtil.hpp"


BaseEncryptApiHandler::BaseEncryptApiHandler(const string &uri) : _uri(uri) {
    AES_init_ctx(&_aesCtx, (const uint8_t *)g_conf.aesKey.c_str());
}

const string &BaseEncryptApiHandler::getUriPath() const {
    return _uri;
}

int BaseEncryptApiHandler::onRequestHeader(HttpConnectionPtr connection) {
    if (connection->request().methodID != HttpMethod::METHOD_POST) {
        auto &response = connection->response();
        response.statusCode = HttpStatusCode::NOT_FOUND;
        response.body = "404 NOT-FOUND-CRYPT";
        response.sendAll();
    }
    return ERR_OK;
}

int BaseEncryptApiHandler::onRequestBody(HttpConnectionPtr connection) {
    auto &bodyEncryptedStr = connection->request().body;

    auto bodyStr = aesDecrypt(_aesCtx, bodyEncryptedStr);
    
    DbApiCtx ctx;
    RapidjsonWriterEx writer;

    writer.startObject();

    rapidjson::Document &body = ctx.body;
    if (bodyStr.empty()) {
        ctx.result = "DECRYPT-FAILED";
        ctx.message = "Failed to decrypt post data.";
        LOG(INFO) << "Failed to decrypt post data, length: " << bodyEncryptedStr.size() << ", data:" << hexToStr(bodyEncryptedStr.substr(0, 200).c_str());
    } else if (body.Parse(bodyStr.c_str(), bodyStr.size()).HasParseError() || !body.IsObject()) {
        ctx.result = "BAD-MESSAGE-FORMAT";
        ctx.message = "Post body should be json format.";
        LOG(INFO) << "Post body should be json format: " << bodyStr.substr(0, 200);
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
    response.body =  aesEncrypt(_aesCtx, StringView(writer.getString(), writer.getSize()));
    response.sendAll();

    return ERR_OK;
}
