//
//  DataSyncLog.hpp
//

#ifndef DataSyncLog_hpp
#define DataSyncLog_hpp

#include <string>
#include "LyricsInfo.hpp"


enum DataSyncAction {
    DSA_CREATE,
    DSA_UPDATE,
    DSA_DELETE,
};

void dslWriteLyricsFile(FilePtr &fp, const std::string &lyrContent, const std::string &fileLink, DataSyncAction action);
void dslWriteDbLyrics(FilePtr &fp, const LyricsInfo &lyrProp, DataSyncAction action);

#endif /* DataSyncLog_hpp */
