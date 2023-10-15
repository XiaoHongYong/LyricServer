//
//  LyricsFileHandler.hpp
//

#pragma once

#ifndef LyricsFileHandler_hpp
#define LyricsFileHandler_hpp

#include "BaseJsonApiHandler.hpp"


/**
 * 内部使用歌词文件相关 API
 */
class LyricsFileHandler : public BaseJsonApiHandler {
public:
    LyricsFileHandler(LyricsServer *server);

protected:
    void handleApi(DbApiCtx &ctx, RapidjsonWriterEx &writer) override;

    LyricsServer                    *_server = nullptr;

};

#endif /* LyricsFileHandler_hpp */
