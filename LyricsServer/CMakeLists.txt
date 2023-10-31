cmake_minimum_required(VERSION 3.15)

if(UNIX AND NOT APPLE)
    set(LINUX TRUE)
endif()

set(CMAKE_CXX_FLAGS "-std=c++17")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(UT "unit test" OFF)
if (UT)
    add_definitions(-DUNIT_TEST)
endif(UT)

if (APPLE)
    add_definitions(-D_MAC_OS)
elseif (LINUX)
    add_definitions(-D_LINUX)
endif (APPLE)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")

PROJECT(lyrics-server)

include_directories(
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/../../third-parties/googletest/include
    ${PROJECT_SOURCE_DIR}/../../third-parties/http-parser
    ${PROJECT_SOURCE_DIR}/../../third-parties/libuv/include
    ${PROJECT_SOURCE_DIR}/../../third-parties/rapidjson
    ${PROJECT_SOURCE_DIR}/../../third-parties/glog/mac
    ${PROJECT_SOURCE_DIR}/../../third-parties/tiny-AES-c
    ${PROJECT_SOURCE_DIR}/../../TinyJS
    ${PROJECT_SOURCE_DIR}/../../third-parties/googletest
    ${PROJECT_SOURCE_DIR}/../..
)

set(SRC 
    ${PROJECT_SOURCE_DIR}/../../LyricsLib/LyricsKeywordFilter.cpp
    ${PROJECT_SOURCE_DIR}/../../MediaTags/LrcParser.cpp
    ${PROJECT_SOURCE_DIR}/../../MediaTags/LyricsData.cpp
    ${PROJECT_SOURCE_DIR}/../../MediaTags/LyrTimestamps.cpp
    ${PROJECT_SOURCE_DIR}/../../third-parties/tiny-AES-c/aes.c
    ${PROJECT_SOURCE_DIR}/../../third-parties/zlib/crc32.c
    ${PROJECT_SOURCE_DIR}/../../third-parties/http-parser/http_parser.c
    ${PROJECT_SOURCE_DIR}/../../third-parties/googletest/src/gtest-all.cc
)

if (UT)
    set(SRC
        ${SRC}
        ${PROJECT_SOURCE_DIR}/../../third-parties/googletest/src/gtest-all.cc
    )
endif(UT)

aux_source_directory(${PROJECT_SOURCE_DIR} SRC)
list(REMOVE_ITEM SRC "${PROJECT_SOURCE_DIR}/main.cpp") # 此文件需要被排除
aux_source_directory(${PROJECT_SOURCE_DIR}/apis SRC)
aux_source_directory(${PROJECT_SOURCE_DIR}/tool SRC)
aux_source_directory(${PROJECT_SOURCE_DIR}/../../HttpLib SRC)
aux_source_directory(${PROJECT_SOURCE_DIR}/../../HttpLib/HttpServer SRC)
aux_source_directory(${PROJECT_SOURCE_DIR}/../../Utils SRC)
list(REMOVE_ITEM SRC "${PROJECT_SOURCE_DIR}/../../Utils/LocalizeTool.cpp") # 此文件需要被排除
aux_source_directory(${PROJECT_SOURCE_DIR}/../../TinyJS/utils SRC)
aux_source_directory(${PROJECT_SOURCE_DIR}/../../MLProtocol SRC)
aux_source_directory(${PROJECT_SOURCE_DIR}/../../third-parties/md5 SRC)
aux_source_directory(${PROJECT_SOURCE_DIR}/../../third-parties/sqlite SRC)
# message(SRC="${SRC}")

set(LINK_OPT_FRAME_WORK)

if (APPLE)
    aux_source_directory(${PROJECT_SOURCE_DIR}/../../Utils/mac SRC)
    aux_source_directory(${PROJECT_SOURCE_DIR}/../../TinyJS/utils/mac SRC)
    set(LINK_OPT_FRAME_WORK "-framework CoreFoundation" "-framework Cocoa")
elseif (LINUX)
    aux_source_directory(${PROJECT_SOURCE_DIR}/../../Utils/linux SRC)
    aux_source_directory(${PROJECT_SOURCE_DIR}/../../TinyJS/utils/linux SRC)
endif (APPLE)

add_subdirectory(${PROJECT_SOURCE_DIR}/../../third-parties/glog glog)

add_subdirectory(${PROJECT_SOURCE_DIR}/../../third-parties/libuv uv_a)

# set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)
set(SRC_SERVER
    ${SRC}
    ${PROJECT_SOURCE_DIR}/main.cpp
)
add_executable(${PROJECT_NAME} ${SRC_SERVER})
target_link_libraries(${PROJECT_NAME} PRIVATE pthread glog uv_a -static-libstdc++ ${LINK_OPT_FRAME_WORK})

set(SRC_CLIENT
    ${SRC}
    ${PROJECT_SOURCE_DIR}/../tests/client.cpp
    ${PROJECT_SOURCE_DIR}/../../MPlayerUI/MLClientSession.cpp
    ${PROJECT_SOURCE_DIR}/../../MPlayerUI/MLProfile.cpp
)
add_executable(client ${SRC_CLIENT})
target_link_libraries(client PRIVATE pthread glog uv_a -static-libstdc++ ${LINK_OPT_FRAME_WORK})
