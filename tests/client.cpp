//
//  client.cpp
//

#include <gtest/gtest.h>
#include "Types.h"
#include "LyricsServer.h"
#include "../../MPlayerUI/MLClientSession.h"
#include "../../MediaTags/LrcParser.h"


/**
 * 此文件用于模拟客户端发送 API 数据.
 */


string setLyricsId(const StringView &data, const string &lyricsId, bool isLrcTag);

extern CProfile g_profile;

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("client PORT_SEARCH_LYRICS FILE_NAME_LYRICS.\n");
        return 1;
    }

    string path = fileGetPath(argv[0]);
    int port = atoi(argv[1]);
    string fnLyrics = argv[2];

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
    g_conf.uploadDirName = g_profile.getString("upload-dir-name", "lu8");
    g_conf.lyricsHttpUrlBase = g_profile.getString("http-lyrics-url-base", "http://search.crintsoft.com/l/");

    google::SetLogDestination(google::WARNING, "");
    google::SetLogDestination(google::ERROR, "");
    google::EnableLogCleaner(30);
    google::InstallFailureSignalHandler();
    google::InstallFailureWriter([](const char* data, size_t size) {
        LOG(INFO).write(data, size);
    });

    fLI::FLAGS_logbuflevel = -1; // 不要内存缓存，直接输出到日志文件
    google::LogToStderr();

    CMLClientSession session;
    session.init("DH-Player");

    // 重新设置为测试端口.
    g_nPortSearchServer = port;

    int ret = session.connect();
    if (ret != ERR_OK) {
        printf("Connect failed.\n");
        return ret;
    }

    session.setUploader("_user_xyz_", "_password_", false);

    // 登录
    string message;
    ret = session.login(message);
    if (ret != ERR_OK) {
        printf("Login failed: %d, %s\n", ret, message.c_str());
        return ret;
    }

    printf("Login successfully.\n");

    string lyrics;
    if (!readFile(fnLyrics.c_str(), lyrics)) {
        printf("Failed to open lyrics file: %d, %s\n", ret, fnLyrics.c_str());
        return 1;
    }

    session.close();
    ret = session.connect();
    if (ret != ERR_OK) {
        printf("Connect failed.\n");
        return ret;
    }

    // 上传歌词
    string lyricsId;
    ret = session.upload(lyrics, lyricsId, message);
    if (ret != ERR_OK) {
        printf("Upload failed: %d, %s\n", ret, message.c_str());
        return ret;
    }
    printf("Upload successfully.\n");

    LyricsProperties props;
    LyrTagParser parser(props);
    parser.parseLrcAllTags(lyrics);

    session.close();
    ret = session.connect();
    if (ret != ERR_OK) {
        printf("Connect failed.\n");
        return ret;
    }

    // 搜索歌词
    MLMsgRetSearch retSearch;
    ret = session.search(true, props.artist.c_str(), props.title.c_str(), 0, retSearch);
    if (ret != ERR_OK) {
        printf("Search lyrics failed: %d, %s\n", ret, retSearch.strMessage.c_str());
        return ret;
    }

    if (retSearch.listResultFiles.size() != 1) {
        printf("Expected returned lyrics count is 1, not: %d\n", (int)retSearch.listResultFiles.size());
        return 1;
    }

    printf("Search lyrics successfully.\n");

    // 测试重复上传
    lyrics = setLyricsId(lyrics, lyricsId, true);
    string lyricsIdNew;
    ret = session.upload(lyrics, lyricsIdNew, message);
    if (ret != ERR_UPLOAD_EXIST) {
        printf("Update lyrics should report upload-exists, not: %d\n", ret);
        return 1;
    }
    printf("Test re-upload existing lyrics successfully.\n");

    // 测试更新歌词
    lyrics.append("\n[01:24.00]and she couldn't find how to push through.");
    ret = session.upload(lyrics, lyricsIdNew, message);
    if (ret != ERR_OK) {
        printf("Update lyrics failed: %d, %s\n", ret, message.c_str());
        return 1;
    }
    printf("Update lyrics successfully.\n");

    // 再次搜索，应该只有一首
    retSearch = MLMsgRetSearch();
    ret = session.search(true, props.artist.c_str(), props.title.c_str(), 0, retSearch);
    if (ret != ERR_OK) {
        printf("Search lyrics failed: %d, %s\n", ret, retSearch.strMessage.c_str());
        return ret;
    }

    if (retSearch.listResultFiles.size() != 1) {
        printf("Expected returned lyrics count is 1, not: %d\n", (int)retSearch.listResultFiles.size());
        return 1;
    }

    printf("Search lyrics again successfully.\n");

    return 0;
}
