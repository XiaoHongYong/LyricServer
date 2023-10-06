//
//  main.cpp
//

#include "Types.h"
#include "../../../Utils/Profile.h"
#include "../../../LyricsLib/LyricsKeywordFilter.h"
#include "../../../MediaTags/LrcParser.h"
#include "LyricsServer.h"


int main(int argc, const char * argv[]) {
    string path = fileGetPath(argv[0]);

    CProfile profile;
    profile.init(dirStringJoin(path.c_str(), "lyrics-server.ini").c_str(), "main");

    ServerConfig conf;

    conf.rootDir = profile.getString("root-dir", "");
    conf.lyricsDir = profile.getString("lyrics-dir", dirStringJoin(conf.rootDir, "lyrics").c_str());
    conf.address = "127.0.0.1";
    conf.port = profile.getInt("port", 1212);
    conf.uploadDirName = profile.getString("upload-dir-name", "lu7");
    conf.lyricsHttpUrlBase = profile.getString("http-lyrics-url-base", "http://search.crintsoft.com/l/");

    LyricsServer server;
    int ret = server.init(conf);
    if (ret != ERR_OK) {
        return ret;
    }

    server.run();

    return 0;
}
