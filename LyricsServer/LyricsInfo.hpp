//
//  LyricsInfo.hpp
//

#ifndef LyricsInfo_hpp
#define LyricsInfo_hpp

#include "../../MediaTags/LyricsData.h"


int64_t decryptLyricsID(cstr_t id);
string encryptLyricsID(uint32_t id);

class LyricsInfo : public LyricsProperties {
public:
    string                      arCmp, tiCmp;
    string                      relatedHttpLink;

    long                        uploaderId;
    long                        lyricsID = 0;
    long                        rateTotal = 0;
    long                        rateCount = 0;
    long                        downloadCount = 0;
    time_t                      uploadDate = 0;
    uint32_t                    digest = 0; // 判断此文件是否完全相同

    void set(LyricsProperties &lyrProperties, long uploaderId, cstr_t uploader);

    void parse(const StringView &lyrics);

};

#endif /* LyricsInfo_hpp */
