//
//  DataSyncLog.cpp
//

#include "DataSyncLog.hpp"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "ServerConfig.hpp"


VecStrings enumDataSyncFileNames() {
    VecStrings names;
    FileFind finder;

    if (finder.openDir(g_conf.dataSyncDir.c_str())) {
        while (finder.findNext()) {
            if (!finder.isCurDir()) {
                StringView name(finder.getCurName());
                if (name.len == 4 && name.isNumeric()) {
                    // 只接受以 year 命名的文件
                    names.push_back(finder.getCurName());
                }
            }
        }

        std::sort(names.begin(), names.end());
    }

    return names;
}

string nextDataSyncLogFileName(const string &curFileName) {
    VecStrings names = enumDataSyncFileNames();
    if (names.empty()) {
        return "";
    }

    for (int i = 0; i < curFileName.size(); i++) {
        if (curFileName == names[i]) {
            if (i + 1 < curFileName.size()) {
                return names[i + 1];
            }
        }
    }

    return names.front();
}

/**
 * 返回待同步的文件名: 最后一个文件
 */
string getToSyncDataFilename() {
    VecStrings names = enumDataSyncFileNames();
    if (names.empty()) {
        return "";
    }

    return names.back();
}

bool tryToReopenDataSyncLogByDate(FilePtr &fp) {
    // 每一年使用一个文件名.
    DateTime now;
    if (g_conf.dataSyncLogYearCur == now.year() && fp.isOK()) {
        return true;
    }

    g_conf.dataSyncLogYearCur = now.year();
    g_conf.dataSyncLogFileNameCur = std::to_string(g_conf.dataSyncLogYearCur) + ".log";

    fp.close();
    return fp.open(dirStringJoin(g_conf.dataSyncDir, g_conf.dataSyncLogFileNameCur).c_str(), "a+b");
}

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

    if (!lyrContent.empty()) {
        writer.Key("content");
        writer.String(lyrContent.c_str(), (uint32_t)lyrContent.size());
    }

    writer.EndObject();

    s.Put('\n');
    fp.write(s.GetString(), s.GetSize());
    fp.flush();
}

void dslWriteDbLyricsCreate(rapidjson::Writer<rapidjson::StringBuffer> &writer, const LyricsInfo &lyrProp) {
    writer.Key("fields");
    writer.String("id,artist,artistcmp,title,titlecmp,album,edited_by,media_length,content_type,upload_time,uploader_id,related_link,digest");

    writer.Key("args");
    writer.StartArray();
    writer.Int64(lyrProp.lyricsID);
    writer.String(lyrProp.artist.c_str());
    writer.String(lyrProp.arCmp.c_str());
    writer.String(lyrProp.title.c_str());
    writer.String(lyrProp.tiCmp.c_str());
    writer.String(lyrProp.album.c_str());
    writer.String(lyrProp.by.c_str());
    writer.Int(atoi(lyrProp.mediaLength.c_str()));
    writer.String(lyrProp.lyrContentType == LCT_LRC ? "lrc" : "txt");
    writer.Int64(lyrProp.uploadDate);
    writer.Int64(lyrProp.uploaderId);
    writer.String(lyrProp.relatedHttpLink.c_str());
    writer.Int64(lyrProp.digest);
    writer.EndArray();
}

void dslWriteDbLyricsUpdate(rapidjson::Writer<rapidjson::StringBuffer> &writer, const LyricsInfo &lyrProp) {
    writer.Key("fields");
    writer.String("artist,title,album,edited_by,media_length,upload_time,digest");

    writer.Key("args");
    writer.StartArray();
    writer.String(lyrProp.artist.c_str());
    writer.String(lyrProp.title.c_str());
    writer.String(lyrProp.album.c_str());
    writer.String(lyrProp.by.c_str());
    writer.Int(atoi(lyrProp.mediaLength.c_str()));
    writer.Int64(lyrProp.uploadDate);
    writer.Int64(lyrProp.digest);
    writer.Int64(lyrProp.lyricsID);
    writer.EndArray();
}

void dslWriteDbLyrics(FilePtr &fp, const LyricsInfo &lyrProp, DataSyncAction action) {
    rapidjson::StringBuffer s;
    rapidjson::Writer<rapidjson::StringBuffer> writer(s);

    writer.StartObject();

    writer.Key("type");
    writer.String("db-lyrics");

    writeAction(writer, action);

    if (action == DSA_CREATE) {
        dslWriteDbLyricsCreate(writer, lyrProp);
    } else if (action == DSA_UPDATE) {
        dslWriteDbLyricsUpdate(writer, lyrProp);
    } else {
        assert(0);
    }

    writer.EndObject();

    s.Put('\n');
    fp.write(s.GetString(), s.GetSize());
    fp.flush();
}

void dslWriteDbAction(FilePtr &fp, const string &tableName, const string &action, const string &fields, const rapidjson::Value &args) {
    rapidjson::StringBuffer s;
    rapidjson::Writer<rapidjson::StringBuffer> writer(s);

    writer.StartObject();

    writer.Key("type");
    writer.String(("db-" + tableName).c_str());

    writer.Key("action");
    writer.String(action.c_str());

    writer.Key("fields");
    writer.String(fields.c_str(), (uint32_t)fields.size());

    writer.Key("args");
    args.Accept(writer);

    writer.EndObject();

    s.Put('\n');

    fp.write(s.GetString(), s.GetSize());
    fp.flush();
}
