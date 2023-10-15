//
//  main.cpp
//

#include <gtest/gtest.h>
#include "Types.h"
#include "../../../Utils/Profile.h"
#include "../../../LyricsLib/LyricsKeywordFilter.h"
#include "../../../MediaTags/LrcParser.h"
#include "LyricsServer.h"


extern CProfile g_profile;

int main(int argc, char *argv[]) {
    string path = fileGetPath(argv[0]);

    g_profile.init(dirStringJoin(path.c_str(), "lyrics-server.ini").c_str(), "main");

    google::InitGoogleLogging(argv[0]);

#ifdef UNIT_TEST
    {
        testing::InitGoogleTest(&argc, argv);

        int ret = RUN_ALL_TESTS();
        if (ret != 0) {
            return ret;
        }
    }
#endif

    srand((uint32_t)time(nullptr));

    g_conf.rootDir = g_profile.getString("root-dir", "");
    g_conf.lyricsDir = g_profile.getString("lyrics-dir", dirStringJoin(g_conf.rootDir, "lyrics").c_str());
    g_conf.address = "127.0.0.1";
    g_conf.port = g_profile.getInt("port", 1212);
    g_conf.uploadDirName = g_profile.getString("upload-dir-name", "lu7");
    g_conf.lyricsHttpUrlBase = g_profile.getString("http-lyrics-url-base", "http://search.crintsoft.com/l/");
    g_conf.isMaster = g_profile.getBool("master", false);
    g_conf.aesKey = g_profile.getString("aes-key", "LL70IXXPF99H9RI9BFTUHBO3GA8AAIKX");
    g_conf.syncMasterUrl = g_profile.getString("sync-master-url", "http://search.crintsoft.com/api-i/data-sync");
    g_conf.dataSyncDir = dirStringJoin(g_conf.rootDir, "data-sync-log");
    g_conf.syncDurationInSec = g_profile.getInt("sync-duration", 60);

    g_conf.fnLog = dirStringJoin(g_conf.rootDir, "logs/lyrics-server/lyrics-server.log");
    createDirectoryAll(fileGetPath(g_conf.fnLog.c_str()).c_str());

    google::SetLogDestination(google::INFO, g_conf.fnLog.c_str());
    google::SetLogDestination(google::WARNING, "");
    google::SetLogDestination(google::ERROR, "");
    google::EnableLogCleaner(30);
    google::InstallFailureSignalHandler();
    google::InstallFailureWriter([](const char* data, int size) {
        LOG(INFO).write(data, size);
    });

    fLI::FLAGS_max_log_size = 10;
    fLI::FLAGS_logbuflevel = -1; // 不要内存缓存，直接输出到日志文件

#ifdef _MAC_OS
    FLAGS_alsologtostderr = 1;
#endif

    LOG(INFO) << "Start " << argv[0];

    LyricsServer server;
    int ret = server.init();
    if (ret != ERR_OK) {
        return ret;
    }

    server.run();

    return 0;
}
