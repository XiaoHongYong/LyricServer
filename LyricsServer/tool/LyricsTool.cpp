//
//  LyricsTool.cpp
//

#include "LyricsTool.hpp"
#include "../ServerConfig.hpp"
#include "../../../LyricsLib/LyricsKeywordFilter.h"
#include "../../../MediaTags/LrcParser.h"


int LyricsTool::run(bool isUpdateDigest, bool isCompressLyrics, bool isAddMissingLyrics) {
    _isUpdateDigest = isUpdateDigest;
    _isCompressLyrics = isCompressLyrics;
    _isAddMissingLyrics = isAddMissingLyrics;

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

    string fnFinishedStatus = dirStringJoin(g_conf.rootDir, "digest-status.txt");
    string content;
    readFile(fnFinishedStatus.c_str(), content);

    VecStrings vStrs;
    strSplit(content.c_str(), '\n', vStrs);
    for (auto &s : vStrs) {
        _finishedDirs.insert(s);
    }

    if (!_fpFinishedStatus.open(fnFinishedStatus.c_str(), "a+b")) {
        LOG(ERROR) << "Failed to open file: " << fnFinishedStatus;
        return ERR_OPEN_FILE;
    }

    ret = processDir(g_conf.lyricsDir);
    if (ret != ERR_OK) {
        LOG(ERROR) << "!!! Not finished, exit with error: " << ret;
    } else {
        LOG(INFO) << "### Finished successfully.";
    }

    return ret;
}

int LyricsTool::processDir(const string &path) {
    FileFind find;

    if (_finishedDirs.find(path) != _finishedDirs.end()) {
        LOG(INFO) << "@ Already Done: " << path;
        return ERR_OK;
    }

    if (!find.openDir(path.c_str())) {
        LOG(ERROR) << "Failed to open direcotry: " << path;
        return ERR_OPEN_FILE;
    }

    while (find.findNext()) {
        auto name = find.getCurName();
        string fn = dirStringJoin(path, name);
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
            } else if (name[0] == '.') {
                LOG(INFO) << "Ignore file: " << fn;
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

    _finishedDirs.insert(path);
    _fpFinishedStatus.write(path + "\n");

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

string setLyricsId(const StringView &data, long lyricsId, bool isLrcTag);

int LyricsTool::addMissingLyrics(const string &fn) {
    string content;

    if (!readFile(fn.c_str(), content)) {
        LOG(ERROR) << "Failed to read file: " << fn;
        return ERR_OPEN_FILE;
    }

    content = convertBinLyricsToUtf8(content, false, ED_SYSDEF);

    LyricsInfo lyrInfo;
    lyrInfo.parse(content);

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
