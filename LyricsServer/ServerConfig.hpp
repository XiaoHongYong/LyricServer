//
//  ServerConfig.hpp
//  LyricsServer
//
//  Created by henry_xiao on 2023/10/11.
//

#ifndef ServerConfig_hpp
#define ServerConfig_hpp

#include "Types.h"

struct ServerConfig {
    string              rootDir;        // 服务运行的根目录
    string              lyricsDir;      // 歌词基本目录
    string              uploadDirName;  // 相对于 lyricsDir 的目录名
    string              lyricsHttpUrlBase;  // 歌词下载链接

    string              address;        // 监听地址
    int                 port = 6000;    // 监听端口
    bool                isMaster = true; // master 服务器支持修改数据库 API，其他服务器只能同步数据，不能修改

    string              fnLog;

    string              aesKey;         // 用于加密通信服务的 Key（如数据同步等）
    string              syncMasterUrl;  // 数据同步的 URL
    string              dataSyncDir;    // 数据同步保存的目录
    string              dataSyncLogFileNameCur; // 当前的数据同步文件名
    int                 dataSyncLogYearCur = -1; // 当前的数据同步 year
    int                 syncDurationInSec = 60; // 数据同步的时间间隔

};

extern ServerConfig g_conf;

#endif /* ServerConfig_hpp */
