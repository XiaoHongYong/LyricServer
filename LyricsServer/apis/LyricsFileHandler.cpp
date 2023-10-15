//
//  LyricsFileHandler.cpp
//

#include "LyricsFileHandler.hpp"
#include "Utils/rapidjson.h"
#include <rapidjson/reader.h>


LyricsFileHandler::LyricsFileHandler(LyricsServer *server) : BaseJsonApiHandler("/lyrics-api/"), _server(server) {
}

void LyricsFileHandler::handleApi(DbApiCtx &ctx, RapidjsonWriterEx &writer) {
    auto action = getMemberString(ctx.body, "action");
    if (action == "delete") {
        // Delete lyrics file.
        auto relatedLink = getMemberString(ctx.body, "related-link");
        if (relatedLink.empty()) {
            ctx.result = "BAD-PARAMS";
            ctx.message = "related-link parameter is expected";
        } else {
            _server->deleteLyricsFile(relatedLink);
        }
    } else {
        ctx.result = "INVALID-ACTION";
        ctx.message = "Invalid action: " + action;
    }
}
