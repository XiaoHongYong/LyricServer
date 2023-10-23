//
//  LyricsTool.hpp
//

#ifndef LyricsTool_hpp
#define LyricsTool_hpp

#include "../Types.h"
#include "../LyricsDB.h"
#include "../UserDB.h"


class LyricsTool {
public:
    int run(bool isUpdateDigest, bool isCompressLyrics, bool isAddMissingLyrics);

protected:
    int processDir(const string &path);
    int compressLyricsFile(const string &fn, bool &isChanged);
    int addMissingLyrics(const string &fn);
    int updateLyricsDigest(const string &fn);

protected:
    LyricsDB                    _dbLyrics;
    UserDB                      _dbUser;

    bool                        _isUpdateDigest = true;
    bool                        _isCompressLyrics = true;
    bool                        _isAddMissingLyrics = true;

    SetStrings                  _finishedDirs;
    FilePtr                     _fpFinishedStatus;

};

#endif /* LyricsTool_hpp */
