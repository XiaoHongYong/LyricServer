//
//  DataSyncLog.hpp
//

#ifndef DataSyncLog_hpp
#define DataSyncLog_hpp

#include <string>
#include <rapidjson/document.h>
#include "LyricsInfo.hpp"


enum DataSyncAction {
    DSA_CREATE,
    DSA_UPDATE,
    DSA_DELETE,
};

string nextDataSyncLogFileName(const string &curFileName);
bool tryToReopenDataSyncLogByDate(FilePtr &fp);
string getToSyncDataFilename();

void dslWriteLyricsFile(FilePtr &fp, const std::string &lyrContent, const std::string &fileLink, DataSyncAction action);
void dslWriteDbLyrics(FilePtr &fp, const LyricsInfo &lyrProp, DataSyncAction action);
void dslWriteDbAction(FilePtr &fp, const string &tableName, const string &action, const string &fields, const rapidjson::Value &args);

#endif /* DataSyncLog_hpp */
