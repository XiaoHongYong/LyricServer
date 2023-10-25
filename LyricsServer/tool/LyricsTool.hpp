//
//  LyricsTool.hpp
//

#ifndef LyricsTool_hpp
#define LyricsTool_hpp

#include "uv.h"
#include "../Types.h"
#include "../LyricsDB.h"
#include "../UserDB.h"


class LyricsTool {
public:
    int start();

protected:
    int onRunProcessLyrDir();
    int onRunProcessLogDir();

    int processDir(const string &path);
    int compressLyricsFile(const string &fn, bool &isChanged);
    int addMissingLyrics(const string &fn);
    int updateLyricsDigest(const string &fn);
    int updateLyricsRelatedLink(const string &fn);

protected:
    LyricsDB                    _dbLyrics;
    UserDB                      _dbUser;

    bool                        _isUpdateDigest = true;
    bool                        _isCompressLyrics = true;
    bool                        _isAddMissingLyrics = true;

    SetStrings                  _finishedJobs;
    FilePtr                     _fpFinishedStatus;

    uv_timer_t                  _timerRun;

    // 遍历歌词目录的堆栈
    std::list<FileFind>         _fileFindStack;

    // 遍历日志文件的 FileFind
    VecStrings                  _logFiles;
    string                      _curLogFile;
    VecStrings                  _logLines;

    bool                        _isQuit = false;

};

#endif /* LyricsTool_hpp */
