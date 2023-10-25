//
//  LyricsTool.cpp
//

#include "LyricsTool.hpp"
#include "../ServerConfig.hpp"
#include "../../../LyricsLib/LyricsKeywordFilter.h"
#include "../../../MediaTags/LrcParser.h"


string setLyricsId(const StringView &data, long lyricsId, bool isLrcTag);
cstr_t getRelatedLink(const string &file, const string &baseDir);

VecStrings listDir(const string &path) {
    VecStrings files;

    FileFind finder;

    if (!finder.openDir(path.c_str())) {
        return files;
    }

    while (finder.findNext()) {
        if (!finder.isCurDir()) {
            files.push_back(dirStringJoin(path, finder.getCurName()));
        }
    }

    return files;
}

int LyricsTool::start() {
    CLyricsKeywordFilter::init();

    int ret = _dbLyrics.init(g_conf.rootDir);
    if (ret != ERR_OK) {
        LOG(ERROR) << "Failed to init lyrics db";
        return ret;
    }

    auto fn = dirStringJoin(g_conf.rootDir, "database/users.db");
    ret = _dbUser.init(fn.c_str());
    if (ret != ERR_OK) {
        LOG(ERROR) << "Failed to init user db";
        return ret;
    }

    string fnFinishedStatus = dirStringJoin(g_conf.rootDir, "tool-status.txt");
    string content;
    readFile(fnFinishedStatus.c_str(), content);

    VecStrings vStrs;
    strSplit(content.c_str(), '\n', vStrs);
    for (auto &s : vStrs) {
        _finishedJobs.insert(s);
    }

    if (!_fpFinishedStatus.open(fnFinishedStatus.c_str(), "a+b")) {
        LOG(ERROR) << "Failed to open file: " << fnFinishedStatus;
        return ERR_OPEN_FILE;
    }

    if (!g_conf.toolProcessLyrDirs.empty()) {
        _fileFindStack.push_back(FileFind());
        if (_finishedJobs.find(g_conf.toolProcessLyrDirs) != _finishedJobs.end()) {
            LOG(INFO) << "!!! Finished, already: " << g_conf.toolProcessLyrDirs;
            return ERR_OK;
        }

        if (!_fileFindStack.back().openDir(g_conf.toolProcessLyrDirs.c_str())) {
            LOG(ERROR) << "!!! Not finished, FAILED to open dir: " << g_conf.toolProcessLyrDirs;
            return ERR_FAILED;
        }
    } else if (!g_conf.toolProcessLogDirs.empty()) {
        _logFiles = listDir(g_conf.toolProcessLogDirs);
    }

    uv_timer_init(uv_default_loop(), &_timerRun);
    _timerRun.data = this;
    uv_timer_start(&_timerRun, [](uv_timer_t *timer) {
        auto thiz = static_cast<LyricsTool *>(timer->data);
        if (thiz->_isQuit) {
            return;
        }

        int ret = g_conf.toolProcessLyrDirs.empty() ? thiz->onRunProcessLogDir() : thiz->onRunProcessLyrDir();
        if (ret != ERR_OK) {
            // 出错，退出
            thiz->_isQuit = true;
            LOG(ERROR) << "!!! Not finished, exit with error: " << ret;
        }
    }, 0, 10);

    return ret;
}

int LyricsTool::onRunProcessLyrDir() {
    // 将目录中的歌词重新压缩、计算 crc、添加不在数据库中的歌词
    if (_fileFindStack.empty()) {
        _isQuit = true;
        LOG(INFO) << "### Finished successfully.";
        return ERR_OK;
    }

    FileFind &find = _fileFindStack.back();
    auto &path = find.path();
    if (!find.findNext()) {
        _finishedJobs.insert(path);
        _fpFinishedStatus.write(path + "\n");
        _fpFinishedStatus.flush();

        LOG(INFO) << "# Done: %s" << path;

        _fileFindStack.pop_back();
        return ERR_OK;
    }

    auto name = find.getCurName();
    string fn = dirStringJoin(path, name);
    if (name[0] == '.') {
        LOG(INFO) << "Ignore file: " << fn;
        return ERR_OK;
    }

    if (find.isCurDir()) {
        if (_finishedJobs.find(fn) != _finishedJobs.end()) {
            LOG(INFO) << "@ Already Done: " << fn;
            return ERR_OK;
        }

        _fileFindStack.push_back(FileFind());
        auto &find = _fileFindStack.back();
        if (!find.openDir(fn.c_str())) {
            LOG(ERROR) << "Failed to open direcotry: " << path;
            return ERR_OPEN_FILE;
        }
    } else {
        StringView tmp(name);
        static StringView EXT_HST(".hst"), EXT_DEL(".del");
        if (tmp.endsWith(EXT_HST) || tmp.endsWith(EXT_DEL)) {
            // 删除 .hst, .del
            deleteFile(fn.c_str());
        } else {
            int ret = ERR_OK;
            bool isChanged = false;
            if (_isCompressLyrics) {
                ret = compressLyricsFile(fn, isChanged);
            }

            if (ret == ERR_OK && _isAddMissingLyrics) {
                ret = addMissingLyrics(fn);
                if (ret == ERR_OK) {
                    // Add missing lyrics will also update digest.
                    return ERR_OK;
                }
            }

            if (ret == ERR_OK && (_isUpdateDigest || isChanged)) {
                ret = updateLyricsDigest(fn);
            }

            if (ret != ERR_OK) {
                return ERR_FAILED;
            }
        }
    }

    return ERR_OK;
}

int LyricsTool::onRunProcessLogDir() {
    // 处理 onRunProcessLyrDir 生成的日志，将有问题的文件再重新添加、压缩
    while (true) {
        if (_logLines.empty()) {
            if (!_curLogFile.empty()) {
                _finishedJobs.insert(_curLogFile);
                _fpFinishedStatus.write(_curLogFile + "\n");
                _fpFinishedStatus.flush();
                LOG(INFO) << "# Done: %s" << _curLogFile;
            }

            if (_logFiles.empty()) {
                LOG(INFO) << "### Finished successfully.";
                _isQuit = true;
                return ERR_OK;
            }

            _curLogFile = _logFiles.back();
            _logFiles.pop_back();
            if (_finishedJobs.find(_curLogFile) != _finishedJobs.end()) {
                LOG(INFO) << "@ Already Done: " << _curLogFile;
                return ERR_OK;
            }

            string content;
            if (!readFile(_curLogFile.c_str(), content)) {
                LOG(ERROR) << "Failed to read file: " << _curLogFile;
                return ERR_OPEN_FILE;
            }

            StringView(content).splitLines(_logLines);
        }

        string line = _logLines.back();
        _logLines.pop_back();
        StringView lineV(line);
        static StringView EMPTY_LYR("File's artist and title is empty: ");
        static StringView ADD_LYR("Adding lyrics: ");

        string fn;
        int pos = lineV.strstr(EMPTY_LYR);
        if (pos != -1) {
            fn = lineV.substr(pos + EMPTY_LYR.len).toString();
        } else {
            pos = lineV.strstr(ADD_LYR);
            if (pos != -1) {
                fn = lineV.substr(pos + ADD_LYR.len).toString();

                updateLyricsRelatedLink(fn);
                return ERR_OK;
            }
            continue;
        }

        if (!isFileExist(fn.c_str())) {
            LOG(ERROR) << "### Quit, file NOT exists: " << fn;
            exit(1);
            return ERR_NOT_FOUND;
        }

        int ret = ERR_OK;
        bool isChanged = false;
        if (_isCompressLyrics) {
            ret = compressLyricsFile(fn, isChanged);
        }

        if (ret == ERR_OK && _isAddMissingLyrics) {
            ret = addMissingLyrics(fn);
            if (ret == ERR_OK) {
                // Add missing lyrics will also update digest.
                return ERR_OK;
            }
        }

        if (ret == ERR_OK && (_isUpdateDigest || isChanged)) {
            ret = updateLyricsDigest(fn);
        }

        if (ret != ERR_OK) {
            return ERR_FAILED;
        }

        return ERR_OK;
    }
}

int LyricsTool::processDir(const string &path) {
    FileFind find;

    if (_finishedJobs.find(path) != _finishedJobs.end()) {
        LOG(INFO) << "@ Already Done: " << path;
        return ERR_OK;
    }

    if (!find.openDir(path.c_str())) {
        LOG(ERROR) << "Failed to open direcotry: " << path;
        return ERR_OPEN_FILE;
    }

    if (g_conf.isQuit) {
        LOG(ERROR) << "Got exit signal, quit...";
        return ERR_FAILED;
    }

    while (find.findNext()) {
        auto name = find.getCurName();
        string fn = dirStringJoin(path, name);
        if (name[0] == '.') {
            LOG(INFO) << "Ignore file: " << fn;
            continue;
        }

        if (find.isCurDir()) {
            int ret = processDir(fn);
            if (ret != ERR_OK) {
                return ret;
            }
        } else {
            StringView tmp(name);
            static StringView EXT_HST(".hst"), EXT_DEL(".del");
            if (tmp.endsWith(EXT_HST) || tmp.endsWith(EXT_DEL)) {
                // 删除 .hst, .del
                deleteFile(fn.c_str());
            } else {
                int ret = ERR_OK;
                bool isChanged = false;
                if (_isCompressLyrics) {
                    ret = compressLyricsFile(fn, isChanged);
                }

                if (ret == ERR_OK && _isAddMissingLyrics) {
                    ret = addMissingLyrics(fn);
                    if (ret == ERR_OK) {
                        // Add missing lyrics will also update digest.
                        continue;
                    }
                }

                if (ret == ERR_OK && (_isUpdateDigest || isChanged)) {
                    ret = updateLyricsDigest(fn);
                }

                if (ret != ERR_OK) {
                    return ERR_FAILED;
                }
            }
        }
    }

    _finishedJobs.insert(path);
    _fpFinishedStatus.write(path + "\n");
    _fpFinishedStatus.flush();

    LOG(INFO) << "# Done: %s" << path;

    return ERR_OK;
}

int LyricsTool::compressLyricsFile(const string &fn, bool &isChanged) {
    string content;
    if (!readFile(fn.c_str(), content)) {
        LOG(ERROR) << "Failed to read file: " << fn;
        return ERR_OPEN_FILE;
    }

    content = convertBinLyricsToUtf8(content, false, ED_SYSDEF);

    // 只转换是有效的 utf8/ansi 文件内容，避免损坏文件
    if ((isUTF8Encoding(content) || isAnsiStr(content.c_str())) && compressLyrics(content)) {
        autoInsertWithUtf8Bom(content);
        if (!writeFile(fn.c_str(), content)) {
            LOG(ERROR) << "Failed to write file: " << fn;
            return ERR_WRITE_FILE;
        }
        isChanged = true;
    }

    return ERR_OK;
}

int LyricsTool::addMissingLyrics(const string &fn) {
    string content;

    if (!readFile(fn.c_str(), content)) {
        LOG(ERROR) << "Failed to read file: " << fn;
        return ERR_OPEN_FILE;
    }

    content = convertBinLyricsToUtf8(content, false, ED_SYSDEF);

    LyricsInfo lyrInfo;
    lyrInfo.parse(content);
    lyrInfo.relatedHttpLink = getRelatedLink(fn, g_conf.lyricsDir);

    if (lyrInfo.title.empty() && lyrInfo.artist.empty()) {
        LOG(ERROR) << "File's artist and title is empty: " << fn;
        return ERR_OK;
    }

    uint32_t digestOld = 0;
    auto id = decryptLyricsID(lyrInfo.id.c_str());
    int ret = _dbLyrics.getLyricsDigest(lyrInfo.arCmp.c_str(), lyrInfo.tiCmp.c_str(), id, digestOld);
    if (ret == ERR_OK) {
        // 存在
        if (_isUpdateDigest && lyrInfo.digest != digestOld) {
            // 更新数据库中的 digest
            LOG(INFO) << "Updating digest: " << fn;
            return _dbLyrics.updateLyricsDigest(id, lyrInfo.digest);
        }
        return ERR_OK;
    }

    // 不存在，添加
    ret = _dbLyrics.isLyricsExist(lyrInfo.arCmp.c_str(), lyrInfo.tiCmp.c_str(), lyrInfo.digest);
    if (ret == ERR_OK) {
        // 已经存在相同内容的文件，删除文件
        LOG(INFO) << "Lyrics exist in database, remove file: " << fn;
        deleteFile(fn.c_str());
        return ERR_OK;
    } else if (ret == ERR_NOT_FOUND) {
        // 不存在
        LOG(INFO) << "Adding lyrics: " << fn;

        ret = _dbUser.getUserId(lyrInfo.by.c_str(), lyrInfo.uploaderId);
        ret = _dbLyrics.AddLyrics(lyrInfo);

        content = setLyricsId(content, lyrInfo.lyricsID, lyrInfo.lyrContentType >= LCT_LRC);

        // Save lyrics file
        if (!writeFile(fn.c_str(), content)) {
            LOG(INFO) << "Failed to save file: " << fn;
            return ERR_WRITE_FILE;
        }

        return ERR_OK;
    } else {
        return ret;
    }
}

int LyricsTool::updateLyricsDigest(const string &fn) {
    string content;
    if (!readFile(fn.c_str(), content)) {
        LOG(ERROR) << "Failed to read file: " << fn;
        return ERR_OPEN_FILE;
    }

    content = convertBinLyricsToUtf8(content, false, ED_SYSDEF);

    LyricsInfo info;
    info.parse(content);

    if (info.id.empty()) {
        LOG(ERROR) << "NO ID tag in file: " << fn;
        return ERR_BAD_FILE_FORMAT;
    }

    int64_t id = decryptLyricsID(info.id.c_str());
    if (id == -1) {
        LOG(ERROR) << "Failed to parse ID tag: " << info.id << " of file: " << fn;
        return ERR_BAD_FILE_FORMAT;
    }

    int ret = _dbLyrics.updateLyricsDigest(id, (uint32_t)info.digest);
    if (ret == ERR_OK) {
        LOG(INFO) << "Update successfully, id: " << id << ", digest: " << info.digest;
    } else {
        LOG(ERROR) << "Failed to update lyrics digest in DB, id: " << id;
    }

    return ret;
}

int LyricsTool::updateLyricsRelatedLink(const string &fn) {
    string content;
    if (!readFile(fn.c_str(), content)) {
        LOG(ERROR) << "Failed to read file: " << fn;
        return ERR_OPEN_FILE;
    }

    content = convertBinLyricsToUtf8(content, false, ED_SYSDEF);

    LyricsInfo info;
    info.parse(content);

    if (info.id.empty()) {
        LOG(ERROR) << "NO ID tag in file: " << fn;
        return ERR_BAD_FILE_FORMAT;
    }

    int64_t id = decryptLyricsID(info.id.c_str());
    if (id == -1) {
        LOG(ERROR) << "Failed to parse ID tag: " << info.id << " of file: " << fn;
        return ERR_BAD_FILE_FORMAT;
    }

    info.relatedHttpLink = getRelatedLink(fn, g_conf.lyricsDir);
    int ret = _dbLyrics.UpdateLyricsLinkById(id, info.relatedHttpLink.c_str());
    if (ret == ERR_OK) {
        LOG(INFO) << "Update successfully, id: " << id << ", relatedHttpLink: " << info.relatedHttpLink;
    } else {
        LOG(ERROR) << "Failed to update lyrics related link in DB, id: " << id;
    }

    return ret;
}
