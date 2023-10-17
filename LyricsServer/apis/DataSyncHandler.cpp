//
//  DataSyncHandler.cpp
//

#include <rapidjson/reader.h>
#include "DataSyncHandler.hpp"
#include "DatabaseApisHandler.hpp"
#include "Utils/rapidjson.h"
#include "../DataSyncLog.hpp"


DataSyncHandler::DataSyncHandler(LyricsServer *server) : BaseEncryptApiHandler("/api-i/data-sync"), _server(server) {
}

void DataSyncHandler::handleApi(DbApiCtx &ctx, RapidjsonWriterEx &writer) {
    _countRequests++;

    string filename = getMemberString(ctx.body, "filename");
    int64_t offset = getMemberInt64(ctx.body, "offset");

    if (filename.empty()) {
        // 从头开始同步.
        filename = nextDataSyncLogFileName("");
        offset = 0;
    }

    auto fn = dirStringJoin(g_conf.dataSyncDir, filename);
    int64_t length = getFileLength(fn.c_str());
    if (offset == length) {
        if (g_conf.dataSyncLogFileNameCur == filename) {
            // 文件已经同步到最新位置了
            DLOG(INFO) << "Sync to file end already, offset/length: " << offset << ", file: " << fn;
            writer.writePropBool("end", true);
            return;
        } else {
            // 换下一个文件同步
            filename = nextDataSyncLogFileName(filename);
            fn = dirStringJoin(g_conf.dataSyncDir, filename);
            offset = 0;
            DLOG(INFO) << "Sync new file: " << filename;
        }
    }

    writer.writePropString("filename", filename);

    if (-1 == length) {
        ctx.result = "FILE-NOT-FOUND";
        LOG(ERROR) << "Failed to send sync response, NO file: " << filename;
        _countErrors++;
    } else if (offset >= length || offset < 0) {
        ctx.result = "OFFSET-OUT-OF-RANGE";
        LOG(ERROR) << "Failed to sync, offset out of range, offset: " << offset << ", length: " << length;
        _countErrors++;
    } else {
        FilePtr fp;
        if (!fp.open(fn.c_str(), "rb")) {
            ctx.result = "FILE-NOT-FOUND";
            LOG(ERROR) << "Failed to sync, can NOT open file: " << fn;
            _countErrors++;
            return;
        }

        fp.seek(offset, SEEK_SET);
        std::array<char, 1024 * 128> buf;
        long size = fp.read(buf.data(), buf.size());
        if (size > 0) {
            writer.writePropInt64("offset", offset);
            writer.writeKey("data");
            writer.writeString(buf.data(), size);
            writer.writePropInt("size", (int)size);
        }

        writer.writePropBool("end", size != buf.size());
        DLOG(ERROR) << "Send sync data, file: " << filename << ", offset: " << offset << ", size: " << size;
    }
}

void DataSyncHandler::dumpStatus(StatusLog &log) {
    log.writeInt("data-sync-requests", _countRequests);
    log.writeInt("data-sync-errors", _countErrors);

    _countErrors = 0;
    _countRequests = 0;
}
