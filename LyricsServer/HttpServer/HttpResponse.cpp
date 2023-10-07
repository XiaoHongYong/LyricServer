//
//  HttpResponse.cpp
//

#include "HttpResponse.hpp"
#include "HttpConnection.hpp"


const string HEADER_CONTENT_TYPE("Content-Type");
const string HEADER_CONTENT_LENGTH("Content-Length");
const string HEADER_CONNECTION("Connection");


const string &getStatusLine(HttpStatusCode code) {

    static const string ok = "HTTP/1.0 200 OK\r\n";
    static const string created = "HTTP/1.0 201 Created\r\n";
    static const string accepted = "HTTP/1.0 202 Accepted\r\n";
    static const string no_content = "HTTP/1.0 204 No Content\r\n";
    static const string multiple_choices = "HTTP/1.0 300 Multiple Choices\r\n";
    static const string moved_permanently = "HTTP/1.0 301 Moved Permanently\r\n";
    static const string moved_temporarily = "HTTP/1.0 302 Moved Temporarily\r\n";
    static const string not_modified = "HTTP/1.0 304 Not Modified\r\n";
    static const string bad_request = "HTTP/1.0 400 Bad Request\r\n";
    static const string unauthorized = "HTTP/1.0 401 Unauthorized\r\n";
    static const string forbidden = "HTTP/1.0 403 Forbidden\r\n";
    static const string not_found = "HTTP/1.0 404 Not Found\r\n";
    static const string internal_server_error = "HTTP/1.0 500 Internal Server Error\r\n";
    static const string not_implemented = "HTTP/1.0 501 Not Implemented\r\n";
    static const string bad_gateway = "HTTP/1.0 502 Bad Gateway\r\n";
    static const string service_unavailable = "HTTP/1.0 503 Service Unavailable\r\n";
    
    switch (code) {
        case HttpStatusCode::OK: return ok;
        case HttpStatusCode::CREATED: return created;
        case HttpStatusCode::ACCEPTED: return accepted;
        case HttpStatusCode::NO_CONTENT: return no_content;
        case HttpStatusCode::MULTIPLE_CHOICES: return multiple_choices;
        case HttpStatusCode::MOVED_PERMANENTLY: return moved_permanently;
        case HttpStatusCode::MOVED_TEMPORARILY: return moved_temporarily;
        case HttpStatusCode::NOT_MODIFIED: return not_modified;
        case HttpStatusCode::BAD_REQUEST: return bad_request;
        case HttpStatusCode::UNAUTHORIZED: return unauthorized;
        case HttpStatusCode::FORBIDDEN: return forbidden;
        case HttpStatusCode::NOT_FOUND: return not_found;
        case HttpStatusCode::INTERNAL_SERVER_ERROR: return internal_server_error;
        case HttpStatusCode::NOT_IMPLEMENTED: return not_implemented;
        case HttpStatusCode::BAD_GATEWAY: return bad_gateway;
        case HttpStatusCode::SERVICE_UNAVAILABLE: return service_unavailable;

        default:
            assert(0);
            return internal_server_error;
    }
}


HttpResponse::HttpResponse(HttpConnection *connection) : _connection(connection) {
    statusCode = HttpStatusCode::INVALID;
}

int HttpResponse::sendAll() {
    VecConstBuffers buffers = headerToBuffers((int)body.size());

    buffers.push_back(body);

    _connection->send(buffers, true);

    return ERR_OK;
}

int HttpResponse::sendAll(const VecConstBuffers &body) {
    size_t len = 0;

    for (auto buf : body) {
        len += buf.len;
    }
    
    VecConstBuffers buffers = headerToBuffers((int)len);

    buffers.insert(buffers.end(), body.begin(), body.end());

    _connection->send(buffers, true);

    return ERR_OK;
}

int HttpResponse::sendHeader() {
    VecConstBuffers buffers = headerToBuffers((int)-1);

    _connection->send(buffers, false);

    return ERR_OK;
}

VecConstBuffers HttpResponse::headerToBuffers(int bodyLength) {
    static const string NAME_VALUE_SEP = ": ";
    static const string CRLF = "\r\n";
    static const string HEADER_TRANSFER_ENCODING = "Transfer-Encoding";
    static const string ENCODING_CHUNKED = "chunked";

    VecConstBuffers buffers;
    
    buffers.push_back(getStatusLine(statusCode));

    if (bodyLength == -1) {
        headers.insert(headers.begin(), {HEADER_TRANSFER_ENCODING, ENCODING_CHUNKED});
    } else {
        headers.insert(headers.begin(), {HEADER_CONTENT_LENGTH, std::to_string(bodyLength)});
    }

    if (getHeaderByName(headers, HEADER_CONTENT_TYPE) == nullptr) {
        headers.insert(headers.end(), {HEADER_CONTENT_TYPE, "text/plain"});
    }

    if (_connection->isKeepAlive()) {
        headers.insert(headers.end(), {HEADER_CONNECTION, "Keep-Alive"});
    }

    for (auto &header : headers) {
        buffers.push_back(header.name);
        buffers.push_back(NAME_VALUE_SEP);
        buffers.push_back(header.value);
        buffers.push_back(CRLF);
    }

    buffers.push_back(CRLF);

    return buffers;
}

void HttpResponse::sendBodyChunked(const string &body, ConnWriteCallback callback, bool isFinished) {
    static const string CRLF = "\r\n";
    static const string CHUNKED_END_CRLF = "0\r\n\r\n";

    VecConstBuffers buffers;

    // Need to save length to headers, to prevent being recycled.
    headers.push_back({"", stringPrintf("%x\r\n", body.size())});
    buffers.push_back(headers.back().value);
    buffers.push_back(body);
    buffers.push_back(CRLF);

    if (isFinished) {
        buffers.push_back(CHUNKED_END_CRLF);
    }

    _connection->send(buffers, callback, isFinished);
}

void HttpResponse::addHeader(const string &name, const string &value) {
    headers.push_back({name, value});
}
