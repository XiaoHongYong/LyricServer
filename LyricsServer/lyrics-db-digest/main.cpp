//
//  main.cpp
//

#include "../Types.h"
#include "../../../Utils/Profile.h"
#include "../../../LyricsLib/LyricsKeywordFilter.h"
#include "../../../MediaTags/LrcParser.h"
#include "../LyricsDB.h"


CProfile g_profile;
CLog g_log;

class LyricsDBDigest {
public:
    int init(const string &rootDir, const string &lyricsDir) {
        CLyricsKeywordFilter::init();

        int ret = m_dbLyrics.init(rootDir);
        if (ret != ERR_OK) {
            LOG("Failed to init lyrics db");
            return ret;
        }

        m_lyricsDir = lyricsDir;

        string fnFinishedStatus = dirStringJoin(rootDir, "digest-status.txt");
        string content;
        readFile(fnFinishedStatus.c_str(), content);

        VecStrings vStrs;
        strSplit(content.c_str(), '\n', vStrs);
        for (auto &s : vStrs) {
            m_finishedDirs.insert(s);
        }

        if (!m_fpFinishedStatus.open(fnFinishedStatus.c_str(), "a+b")) {
            LOG("Failed to open file: %s", fnFinishedStatus.c_str());
            return ERR_OPEN_FILE;
        }

        return ERR_OK;
    }

    int updateDigest() {

        updateDigest(m_lyricsDir);

        return ERR_OK;
    }

    int updateDigest(const string &path) {
        FileFind find;

        if (m_finishedDirs.find(path) != m_finishedDirs.end()) {
            LOG("@ Already Done: %s", path.c_str());
            return ERR_OK;
        }

        if (!find.openDir(path.c_str())) {
            LOG("Failed to open direcotry: %s", path.c_str());
            return ERR_OPEN_FILE;
        }

        while (find.findNext()) {
            auto name = find.getCurName();
            string fn = dirStringJoin(path, name);
            if (find.isCurDir()) {
                int ret = updateDigest(fn);
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
                    LOG("Ignore file: %s", fn.c_str());
                } else {
                    int ret = updateLyricsDigest(fn);
                    if (ret != ERR_OK) {
                        return ERR_FAILED;
                    }
                }
            }
        }

        m_finishedDirs.insert(path);
        m_fpFinishedStatus.write(path + "\n");

        LOG("# Done: %s", path.c_str());

        return ERR_OK;
    }

    int updateLyricsDigest(const string &fn) {
        string content;
        if (!readFile(fn.c_str(), content)) {
            LOG("Failed to read file: %s", fn.c_str());
            return ERR_OPEN_FILE;
        }

        content = convertBinLyricsToUtf8(content, false, ED_SYSDEF);

        LyricsInfo info;
        info.parse(content);

        if (info.id.empty()) {
            LOG("NO ID tag in file: %s", fn.c_str());
            return ERR_BAD_FILE_FORMAT;
        }

        int64_t id = decryptLyricsID(info.id.c_str());
        if (id == -1) {
            LOG("Failed to parse ID tag: %s of file: %s", info.id.c_str(), fn.c_str());
            return ERR_BAD_FILE_FORMAT;
        }

        LOG("Update id: %d, digest: %u", (int)id, (uint32_t)info.digest);
        int ret = m_dbLyrics.updateLyricsDigest(id, (uint32_t)info.digest);
        if (ret != ERR_OK) {
            LOG("Failed to update lyrics: %lld digest in DB.", id);
        }

        return ret;
    }

protected:
    LyricsDB                    m_dbLyrics;
    string                      m_lyricsDir;

    SetStrings                  m_finishedDirs;
    FilePtr                     m_fpFinishedStatus;

};

int main(int argc, const char * argv[]) {
    string path = fileGetPath(argv[0]);

    CProfile profile;
    profile.init(dirStringJoin(path.c_str(), "lyrics-db-digest.ini").c_str(), "main");

    auto rootDir = profile.getString("root-dir", "");
    auto lyricsDir = profile.getString("lyrics-dir", dirStringJoin(rootDir, "lyrics").c_str());

    LyricsDBDigest tool;
    int ret = tool.init(rootDir, lyricsDir);
    if (ret != ERR_OK) {
        printf("Failed to init, error: %d\n", ret);
        return ret;
    }

    ret = tool.updateDigest();
    if (ret == ERR_OK) {
        printf("### Succeeded.\n");
    } else {
        printf("### Failed.\n");
    }

    return 0;
}
