//
//  main.cpp
//

#include <gtest/gtest.h>
#include <csignal>
#include "Types.h"
#include "../../../Utils/Profile.h"
#include "../../../LyricsLib/LyricsKeywordFilter.h"
#include "../../../MediaTags/LrcParser.h"
#include "LyricsServer.h"
#include "tool/LyricsTool.hpp"


extern CProfile g_profile;

struct CmdOptions {
    bool                isQuit = false;
};


const char *USAGE = R"(Usage: lyrics-server [-h, --help]
                        [--decode-id ID-STR]
                        [--parse-tags FILE]

if no parameter is specified, it's running in service mode, or it will run once and quit.

Option arguments:
  -h, --help             show this message
  --decode-id ID-STR     decode lyrics id of ID-STR
  --parse-tags FILE      parse tag of lyrics FILE
)";


bool parseCmdLine(int argc, char *argv[], CmdOptions &optsOut) {
    for (int i = 1; i < argc; i++) {
        StringView name(argv[i]);
        if (name.equal("--decode-id")) {
            i++;
            if (i >= argc) {
                printf("Missing parameter ID-STR for --decode-id");
                return false;
            }

            auto id = decryptLyricsID(argv[i]);
            printf("Decode lyrics id: %s => %ld\n", argv[i], id);

            optsOut.isQuit = true;
        } else if (name.equal("--parse-tags")) {
            i++;
            if (i >= argc) {
                printf("Missing parameter FILE for --parse-tags");
                return false;
            }

            auto fn = argv[i];
            string content;
            if (!readFile(fn, content)) {
                printf("Failed to read file: %s\n", fn);
                return false;
            }

            content = convertBinLyricsToUtf8(content, false, ED_SYSDEF);

            LyricsInfo lyrInfo;
            lyrInfo.parse(content);

            LyrTagParser tagParser(lyrInfo);
            printf("== Tags of file: %s\n", fn);
            printf("%s\n", tagParser.toLrcTags().c_str());

            optsOut.isQuit = true;
        } else {
            printf("Unkown parameter: %s\n", argv[i]);
            return false;
        }
    }

    return true;
}

int main(int argc, char *argv[]) {
    CmdOptions opts;
    if (!parseCmdLine(argc, argv, opts)) {
        return 1;
    }

    if (opts.isQuit) {
        return 0;
    }

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

    g_conf.toolProcessLyrDirs = g_profile.getString("tool-process-lyr-dir", "");
    g_conf.toolProcessLogDirs = g_profile.getString("tool-process-log-dir", "");
    bool isToolOn = !g_conf.toolProcessLyrDirs.empty() || !g_conf.toolProcessLogDirs.empty();

    g_conf.fnLog = dirStringJoin(g_conf.rootDir,
        isToolOn ? "logs/lyrics-tool.log" : "logs/lyrics-server/lyrics-server.log");
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

    std::signal(SIGTERM, [](int signal) {
        LOG(INFO) << "Got SIGTERM signal, quitting...";
        printf("Got SIGTERM signal, quitting...");
        g_conf.isQuit = true;
        uv_stop(uv_default_loop());
    });

    LOG(INFO) << "Start " << argv[0];

    LyricsTool tool;
    if (isToolOn) {
        tool.start();
    }

    LyricsServer server;
    int ret = server.init();
    if (ret != ERR_OK) {
        return ret;
    }

    server.run();

    return 0;
}
