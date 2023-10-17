//
//  SyncRemoteMasterData.cpp
//

#include "SyncRemoteMasterData.hpp"
#include "ServerConfig.hpp"
#include "AesUtil.hpp"
#include "DataSyncLog.hpp"
#include "LyricsServer.h"
#include "Utils/rapidjson.h"


void SyncRemoteMasterData::start(LyricsServer *server) {
    _server = server;

    uv_timer_init(uv_default_loop(), &_timer);
    _timer.data = this;

    uv_timer_start(&_timer, onTimer, 0, g_conf.syncDurationInSec * 1000);

    AES_init_ctx(&_aesCtx, (const uint8_t *)g_conf.aesKey.c_str());
}

void SyncRemoteMasterData::onTimer(uv_timer_t *timer) {
    SyncRemoteMasterData *thiz = static_cast<SyncRemoteMasterData *>(timer->data);

    assert(!g_conf.isMaster);
    thiz->sendSyncRequest();
}

void SyncRemoteMasterData::sendSyncRequest() {
    auto filename = getToSyncDataFilename();
    int64_t offset = 0;

    if (!filename.empty()) {
        auto fn = dirStringJoin(g_conf.dataSyncDir, filename);
        offset = getFileLength(fn.c_str());
        if (offset == -1) {
            LOG(ERROR) << "Failed to get file length: " << fn;
            return;
        }
    }

    RapidjsonWriterEx writer;
    writer.startObject();
    writer.writePropString("filename", filename);
    writer.writePropInt64("offset", offset);
    writer.endObject();

    string body = aesEncrypt(_aesCtx, StringView(writer.getString(), writer.getSize()));

    DLOG(INFO) << "Send sync request: " << g_conf.syncMasterUrl << "body: " << writer.getString();

    _client.post(g_conf.syncMasterUrl,
            {}, body, [this](int statusCode, const ListHttpHeaders &headers, const string &body) {
        if (statusCode == 200) {
            string decrypted = aesDecrypt(_aesCtx, body);
            if (decrypted.empty()) {
                LOG(ERROR) << "Failed to decrypt data.";
                return;
            }

            DLOG(INFO) << "onSync: " << decrypted;

            rapidjson::Document doc;
            if (doc.Parse(decrypted.c_str(), decrypted.size()).HasParseError() || !doc.IsObject()) {
                LOG(ERROR) << "Failed to parse document.";
                return;
            }

            onSync(doc);
        } else {
            LOG(ERROR) << "Data sync return http statusCode: " << statusCode << ", data: " << body.substr(0, 100);
        }
    });
}

void SyncRemoteMasterData::onSync(rapidjson::Value &body) {
    string result = getMemberString(body, "result");
    if (result != "OK") {
        LOG(ERROR) << "result is NOT OK: " << result;
        return;
    }

    string filename = getMemberString(body, "filename");
    auto end = getMemberBool(body, "end");
    string data = getMemberString(body, "data");
    if (data.empty()) {
        assert(end);
        LOG(INFO) << "No data to update";
        return;
    }

    auto offset = getMemberInt64(body, "offset");
    auto size = getMemberInt(body, "size");

    assert(data.size() == size);
    if (data.size() != size) {
        // JSON 编码后的数据大小不正确？
        LOG(ERROR) << "returned size: " << size << " is NOT same as data size: " << data.size();
        return;
    }

    FilePtr fp;

    auto fn = dirStringJoin(g_conf.dataSyncDir, filename);
    if (!fp.open(fn, "a+b")) {
        LOG(ERROR) << "Failed to open file: " << fn;
        return;
    }

    if (fp.fileSize() != offset) {
        // 同步的位置不正确
        LOG(ERROR) << "Failed to sync file: " << fn << ", size: " << fp.fileSize() << " not match offset: " << offset;
        return;
    }

    if (fp.write(data) != data.size()) {
        // 写入的数据大小不正确
        LOG(ERROR) << "Failed to write file: " << fn;
        return;
    }

    fp.close();

    _server->doDataSync(filename);

    if (!end) {
        // 继续同步.
        DLOG(ERROR) << "NOT finished, do sync again";
        sendSyncRequest();
    }
}
