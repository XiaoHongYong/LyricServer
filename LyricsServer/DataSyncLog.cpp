//
//  DataSyncLog.cpp
//

#include "DataSyncLog.hpp"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"


void writeAction(rapidjson::Writer<rapidjson::StringBuffer> &writer, DataSyncAction action) {
    writer.Key("action");
    switch (action) {
        case DSA_CREATE:
            writer.String("create");
            break;
        case DSA_UPDATE:
            writer.String("update");
            break;
        case DSA_DELETE:
            writer.String("delete");
            break;
        default:
            assert(0);
            break;
    }
}

void dslWriteLyricsFile(FilePtr &fp, const std::string &lyrContent, const std::string &fileLink, DataSyncAction action) {
    rapidjson::StringBuffer s;
    rapidjson::Writer<rapidjson::StringBuffer> writer(s);

    writer.StartObject();

    writer.Key("type");
    writer.String("file-lyrics");

    writeAction(writer, action);

    writer.Key("name");
    writer.String(fileLink.c_str());

    writer.Key("content");
    writer.String(lyrContent.c_str(), (uint32_t)lyrContent.size());

    writer.EndObject();

    s.Put('\n');
    fp.write(s.GetString(), s.GetSize());
    fp.flush();
}

void dslWriteDbLyrics(FilePtr &fp, const LyricsInfo &lyrProp, DataSyncAction action) {
    rapidjson::StringBuffer s;
    rapidjson::Writer<rapidjson::StringBuffer> writer(s);

    writer.StartObject();

    writer.Key("type");
    writer.String("db-lyrics");

    writeAction(writer, action);

    writer.Key("id");
    writer.Int64(lyrProp.lyricsID);

    writer.Key("ar");
    writer.String(lyrProp.artist.c_str());

    writer.Key("ti");
    writer.String(lyrProp.title.c_str());

    writer.Key("al");
    writer.String(lyrProp.album.c_str());

    writer.Key("by");
    writer.String(lyrProp.by.c_str());

    if (!lyrProp.mediaLength.empty()) {
        writer.Key("duration");
        writer.String(lyrProp.mediaLength.c_str());
    }

    writer.Key("content-type");
    writer.String(lyrProp.lyrContentType == LCT_LRC ? "lrc" : "txt");

    writer.Key("digest");
    writer.Int64(lyrProp.digest);

    writer.Key("uploader-id");
    writer.Int64(lyrProp.uploaderId);

    writer.EndObject();

    s.Put('\n');
    fp.write(s.GetString(), s.GetSize());
    fp.flush();
}
