#include <sys/resource.h>
#include "Types.h"
#include "LyricsServer.h"
#include "LyricsLib/LyricsKeywordFilter.h"
#include "Utils/rapidjson.h"
#include "UserDB.h"
#include "SpamLyricsFilter.h"
#include "DataSyncLog.hpp"
#include "apis/ClientApisHandler.hpp"
#include "apis/StatusHandler.hpp"
#include "apis/DatabaseApisHandler.hpp"
#include "apis/DbModifyHandler.hpp"
#include "apis/LyricsFileHandler.hpp"
#include "apis/DataSyncHandler.hpp"


CProfile g_profile;
CLog g_log;

const int SAME_LYR_EXISTS_MAX = 8;

cstr_t GenRandLyrFilePrefix() {
    static char str[4];

    str[0] = 'a' + rand() % ('z' - 'a');
    str[1] = 'a' + rand() % ('z' - 'a');
    str[2] = '_';
    str[3] = '\0';

    return str;
};

void getRelatedDir(cstr_t szDir, cstr_t szBaseDir, char *szRelatedDir) {
    auto nBaseLen = strlen(szBaseDir);
    if (strncasecmp(szBaseDir, szDir, nBaseLen) == 0) {
        if (szDir[nBaseLen] == PATH_SEP_CHAR) {
            nBaseLen++;
        }
        strcpy(szRelatedDir, szDir + nBaseLen);
    } else {
        assert(0 && "GetRelatedPath()");
        strcpy(szRelatedDir, szDir);
    }
}

cstr_t GenRandLyrFilePrefix();

int iFindAtLineStart(const StringView &data, const StringView &pattern) {
    if (data.iStartsWith(pattern)) {
        return 0;
    }

    int start = 0;
    while (true) {
        start = data.strchr('\n', start);
        if (start == -1) {
            return -1;
        }
        start++;

        auto line = data.substr(start);
        if (line.iStartsWith(pattern)) {
            return start;
        }
    }
}

bool isWinNewLine(const StringView &data) {
    auto p = data.data, end = data.data + data.len;
    for (; p < end; p++) {
        if (*p == '\n') {
            return false;
        } else if (*p == '\r') {
            return true;
        }
    }

    return false;
}

string setLyricsId(const StringView &data, const string &lyricsId, bool isLrcTag) {
    static StringView LRC_ID("[id:");
    static StringView TXT_ID("id:");

    // int pos = -1, end = -1;
    int pos = iFindAtLineStart(data, isLrcTag ? LRC_ID : TXT_ID);
    if (pos != -1) {
        int end = -1;
        if (isLrcTag) {
            int end1 = data.strchr(']', pos);
            int end2 = data.strchr('\n', pos + 1);
            if (end1 != -1 && end1 < end2) {
                // 结束的 ']' 必须在此行内
                pos += LRC_ID.len;
                end = end1;
            }
        } else {
            end = data.strchr('\n', pos);
            if (end != -1) {
                pos += LRC_ID.len;
            }
        }

        if (end != -1) {
            string lyrics;

            lyrics.append(data.data, pos);
            lyrics.push_back(' ');
            lyrics.append(lyricsId);
            lyrics.append(data.data + end, data.len - end);

            return lyrics;
        }
    }

    string lyrics;
    if (isLrcTag) {
        // 添加到开头
        lyrics.append("[id: ");
        lyrics.append(lyricsId);
        lyrics.push_back(']');

        if (isWinNewLine(data)) {
            lyrics.append("\r\n");
        } else {
            lyrics.push_back('\n');
        }

        lyrics.append(data.data, data.len);
    } else {
        // 添加到结尾
        lyrics.append(data.data, data.len);
        if (isWinNewLine(data)) {
            lyrics.append("\r\n");
        } else {
            lyrics.push_back('\n');
        }

        lyrics.append("id: ");
        lyrics.append(lyricsId);
    }

    return lyrics;
}

string setLyricsId(const StringView &data, long lyricsId, bool isLrcTag) {
    return setLyricsId(data, encryptLyricsID(lyricsId), isLrcTag);
}

//////////////////////////////////////////////////////////////////////////
// LyricsServer

LyricsServer::LyricsServer() {
    _startTime = time(NULL);
    srand((int)_startTime);
}

LyricsServer::~LyricsServer() {
}

int LyricsServer::init() {
    dirStringAddSep(g_conf.lyricsDir);
    dirStringAddSep(g_conf.uploadDirName);

    CLyricsKeywordFilter::init();

    int ret = _dbLyrics.init(g_conf.rootDir);
    if (ret != ERR_OK) {
        LOG(ERROR) << "Failed to init lyrics db";
        return ret;
    }

    auto fn = dirStringJoin(g_conf.rootDir, "database/users.db");
    ret = _dbUser.init(fn.c_str());
    if (ret != ERR_OK) {
        LOG(ERROR) << "Failed to init users db";
        return ret;
    }

    if (g_conf.isMaster) {
        if (!tryToReopenDataSyncLogByDate(_fpSyncLog)) {
            LOG(ERROR) << "Failed to open data-sync log: " << getToSyncDataFilename();
            return ERR_OPEN_FILE;
        }
    } else {
        // Slave 节点
        _syncRemoteMasterData.start(this);
        _dbModifierUsers.init(_dbUser.db(), "users");
        _dbModifierLyrics.init(_dbLyrics.db(), "lyrics");
    }

    _statusLog.init(dirStringJoin(g_conf.rootDir, "logs/lyr-svr-status.log").c_str(),
                    g_conf.statusLogRotateSize, g_conf.statusLogCount);

    HttpServer::init(g_conf.address, g_conf.port);

    auto clientApisHandler = std::make_shared<ClientApisHandler>(this);
    registerRequestHandler(clientApisHandler);
    _clientApisHandler = clientApisHandler.get();

    registerRequestHandler(std::make_shared<DatabaseApisHandler>(_dbUser.db(), "/db-api/users"));
    registerRequestHandler(std::make_shared<DatabaseApisHandler>(_dbLyrics.db(), "/db-api/lyrics"));

    if (g_conf.isMaster) {
        // 只有 master 才能提供修改数据库的接口
        registerRequestHandler(std::make_shared<DbModifyHandler>(this, _dbUser.db(), "users", "/db-modify-api/users"));
        registerRequestHandler(std::make_shared<DbModifyHandler>(this, _dbLyrics.db(), "lyrics", "/db-modify-api/lyrics"));
        registerRequestHandler(std::make_shared<LyricsFileHandler>(this));

        // 数据同步服务接口
        registerRequestHandler(std::make_shared<DataSyncHandler>(this));
    }

    registerRequestHandler(std::make_shared<StatusHandler>(this));
    registerRequestHandler(std::make_shared<RunningHandler>());

    uv_timer_init(uv_default_loop(), &_timerLogStatus);
    _timerLogStatus.data = this;
    uv_timer_start(&_timerLogStatus, onTimerLogStatus, 0, g_conf.statusLogDurationInSec * 1000);

    return ERR_OK;
}

// New uploaded lyrics.
int LyricsServer::addNewLyrics(LyricsInfo &lyrInfo, string &strLyrContent) {
    // Add lyrics in DB first
    int nRet = _dbLyrics.AddLyrics(lyrInfo);
    if (nRet != ERR_OK) {
        return nRet;
    }
    assert(lyrInfo.lyricsID != 0);

    strLyrContent = setLyricsId(strLyrContent, lyrInfo.lyricsID, lyrInfo.lyrContentType >= LCT_LRC);

    // Save lyrics file
    nRet = saveLyricsFile(lyrInfo, strLyrContent);
    if (nRet != ERR_OK) {
        return nRet;
    }

    dslWriteLyricsFile(_fpSyncLog, strLyrContent, lyrInfo.relatedHttpLink, DSA_CREATE);
    dslWriteDbLyrics(_fpSyncLog, lyrInfo, DSA_CREATE);

    return _dbLyrics.UpdateLyricsLinkById(lyrInfo.lyricsID, lyrInfo.relatedHttpLink.c_str());
}

int LyricsServer::updateLyrics(LyricsInfo &lyrInfo, string &strLyrContent) {
    strLyrContent = setLyricsId(strLyrContent, lyrInfo.lyricsID, lyrInfo.lyrContentType >= LCT_LRC);

    string strFile = g_conf.lyricsDir + lyrInfo.relatedHttpLink;
    if (!writeFile(strFile.c_str(), strLyrContent)) {
        return ERR_OPEN_FILE;
    }

    int ret = _dbLyrics.UpdateLyricsPropById(lyrInfo.lyricsID, lyrInfo);
    if (ret != ERR_OK) {
        return ret;
    }

    dslWriteLyricsFile(_fpSyncLog, strLyrContent, lyrInfo.relatedHttpLink, DSA_UPDATE);
    dslWriteDbLyrics(_fpSyncLog, lyrInfo, DSA_UPDATE);

    DLOG(INFO) << "Update lyrics successfully.";

    return ERR_OK;
}

int LyricsServer::saveLyricsFile(LyricsInfo &lyrInfo, string &strLyrContentUtf8) {
    string strFile = g_conf.lyricsDir + g_conf.uploadDirName;

    {
        time_t now = time(NULL);
        tm *t = localtime(&now);

        char szTemp[64];
        sprintf(szTemp, "%02d%02d%02d/", t->tm_year + 1900 - 2000, t->tm_mon + 1, t->tm_mday);
        strFile += szTemp;

        if (!isDirExist(strFile.c_str())) {
            if (!createDirectoryAll(strFile.c_str())) {
                LOG(ERROR) << "Failed to create directory: " << strFile;
            }
        }
    }

    strFile += GenRandLyrFilePrefix();
    strFile += encryptLyricsID(lyrInfo.lyricsID);
    strFile += lyrInfo.lyrContentType == LCT_TXT ? ".txt" : ".lrc";

    // Update file link in lyrics db.
    char szRelatedLink[MAX_PATH];
    getRelatedDir(strFile.c_str(), g_conf.lyricsDir.c_str(), szRelatedLink);
    strrep(szRelatedLink, '\\', '/');
    lyrInfo.relatedHttpLink = szRelatedLink;

    if (!writeFile(strFile.c_str(), strLyrContentUtf8)) {
        LOG(ERROR) << "Failed to saved lyrics file: " << strFile;
        return ERR_OPEN_FILE;
    }

    DLOG(INFO) << "Saved lyrics file successfully at: " << strFile;

    return ERR_OK;
}

int LyricsServer::deleteLyricsFile(const string &relatedLink) {
    string fn = dirStringJoin(g_conf.lyricsDir, relatedLink);
    if (isFileExist(fn.c_str())) {
        deleteFile(fn.c_str());

        DLOG(INFO) << "Delete lyrics file successfully: " << fn;

        dslWriteLyricsFile(_fpSyncLog, "", relatedLink, DSA_DELETE);
        return ERR_OK;
    } else {
        LOG(INFO) << "Failed to delete lyrics file, not exist: " << fn;
        return ERR_NOT_FOUND;
    }
}

void LyricsServer::doDataSync(const string &filename) {
    string fn = dirStringJoin(g_conf.dataSyncDir, filename);
    assert(isFileExist(fn.c_str()));

    FilePtr fp;

    if (!fp.open(fn.c_str(), "rb")) {
        LOG(ERROR) << "Failed to open data-sync-file: " << fn;
        return;
    }

    int pos = g_profile.getInt("data-sync-pos", filename.c_str(), 0);

    auto fileSize = fp.fileSize();
    if (pos > fileSize) {
        LOG(ERROR) << "data-sync position: " << pos << " is less than file size: " << fileSize << " file: " << filename;
        return;
    } else if (pos == fileSize) {
        LOG(INFO) << "Already executed sync to latest position/fileSize: " << pos;
        return;
    }

    DLOG(INFO) << "Execute sync from position: " << pos << ", file size: " << fileSize << ", file: " << fn;

    std::array<char, 1024 * 1024> buf;

    while (!fp.isEof()) {
        fp.seek(pos, SEEK_SET);
        auto len = fp.read(buf.data(), buf.size());
        StringView str(buf.data(), len);
        VecStringViews lines;
        str.split('\n', lines);

        if (!lines.empty()) {
            // 去掉最后一行不完整的数据、或者空行.
            lines.pop_back();
        }

        if (lines.empty()) {
            break;
        }

        int i = 0;
        for (auto &line : lines) {
            executeDataSync(line);
            pos += line.len + 1;

            i++;
            if (i % 10 == 0) {
                // 每 10 行写一次进度，避免异常丢失进度
                g_profile.writeInt("data-sync-pos", filename.c_str(), pos);
            }
        }

        g_profile.writeInt("data-sync-pos", filename.c_str(), pos);
    }
}

bool LyricsServer::executeDataSync(const StringView &line) {
    rapidjson::Document doc;
    if (doc.Parse(line.data, line.len).HasParseError() || !doc.IsObject()) {
        LOG(ERROR) << "Failed to parse data sync line: " << line.toString();
        return false;
    }

    DLOG(INFO) << "executeDataSync: " << line.toString();

    auto type = getMemberString(doc, "type");
    auto action = getMemberString(doc, "action");

    static string ACT_CREATE("create"), ACT_UPDATE("update"), ACT_DELETE("delete");

    if (type == "file-lyrics") {
        auto name = getMemberString(doc, "name");
        if (name.empty()) {
            LOG(ERROR) << "name field is required.";
            return false;
        }

        auto fn = dirStringJoin(g_conf.lyricsDir, name);
        if (action == ACT_CREATE || action == ACT_UPDATE) {
            auto content = getMemberString(doc, "content");
            if (content.empty()) {
                LOG(ERROR) << "content field is required.";
                return false;
            }

            if (!writeFile(fn.c_str(), content)) {
                auto path = fileGetPath(fn.c_str());
                if (!isDirExist(path.c_str())) {
                    createDirectoryAll(path.c_str());
                    if (!writeFile(fn.c_str(), content)) {
                        LOG(ERROR) << "Failed to write file:" << fn;
                        return false;
                    }
                }
            }

            DLOG(INFO) << "executeDataSync, save lyrics file successfully:" << fn;
        } else if (action == ACT_DELETE) {
            if (isFileExist(fn.c_str())) {
                if (!deleteFile(fn.c_str())) {
                    LOG(ERROR) << "Failed to delete file:" << fn;
                    return false;
                }
            }
            DLOG(INFO) << "executeDataSync, delete lyrics file successfully:" << fn;
        } else {
            assert(0);
            LOG(ERROR) << "Unkown action:" << action;
            return false;
        }
        return true;
    } else if (type == "db-lyrics" || type == "db-users") {
        auto fields = getMemberString(doc, "fields");
        auto &args = getMember(doc, "args");
        if (!args.IsArray() || fields.empty()) {
            LOG(ERROR) << "args and fields are required.";
            return false;
        }

        DLOG(INFO) << "Sync database data, type: " << type << ", action: " << action;

        DatabaseModifier &dbModifier = type == "db-lyrics" ? _dbModifierLyrics : _dbModifierUsers;
        DbApiCtx ctx;
        int64_t idReturned = -1;
        return dbModifier.executeAction(ctx, action, fields, args, idReturned);
    } else {
        LOG(ERROR) << "Unkown type:" << type;
        return false;
    }
}

void LyricsServer::dumpStatus(IJsonWriter *writer) {
    writer->startObject();

    writer->writePropTime("StartTime", _startTime);

    writer->writeKey("ClientApi");
    _clientApisHandler->dumpStatus(writer);

    writer->endObject();
}

void LyricsServer::writeStatusLog() {
    _statusLog.startLine();

    struct rusage usage;
    memset(&usage, 0, sizeof(usage));
    getrusage(RUSAGE_SELF, &usage);

    _statusLog.writeInt("time", time(nullptr));

    // 记录此进程在这段时间的 cpu 使用
    auto cpuUsage = usage.ru_utime.tv_sec + usage.ru_stime.tv_sec;
    _statusLog.writeInt("cpu", cpuUsage - _latestCpuUsage);
    _latestCpuUsage = cpuUsage;

    _statusLog.writeInt("memory-data", usage.ru_idrss);
    _statusLog.writeInt("memory-stack", usage.ru_isrss);

    for (auto &handler : handlers()) {
        handler->dumpStatus(_statusLog);
    }

    _statusLog.endLine();
}

void LyricsServer::onTimerLogStatus(uv_timer_t *timer) {
    auto thiz = static_cast<LyricsServer *>(timer->data);

    if (g_conf.isMaster) {
        // 定期检查，根据时间重命名当前同步日志文件
        tryToReopenDataSyncLogByDate(thiz->_fpSyncLog);
    }

    thiz->writeStatusLog();
}
