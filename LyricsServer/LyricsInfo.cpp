//
//  LyricsInfo.cpp
//

#include "LyricsInfo.hpp"
#include "../../LyricsLib/LyricsKeywordFilter.h"
#include "../../MediaTags/LrcParser.h"
#include "../../Utils/Digest.h"


void LyricsInfo::set(LyricsProperties &lyrProperties, long uploaderId, cstr_t uploader) {
    *dynamic_cast<LyricsProperties *>(this) = lyrProperties;

    uploadDate = time(NULL);

    CLyricsKeywordFilter::filter(lyrProperties.artist.c_str(), arCmp);
    CLyricsKeywordFilter::filter(lyrProperties.title.c_str(), tiCmp);

    if (!isEmptyString(uploader)) {
        by = uploader;
    }

    assert(lyrContentType == LCT_LRC || lyrContentType == LCT_TXT);

    this->uploaderId = uploaderId;
}

void LyricsInfo::parse(const StringView &lyrics) {
    VecStringViews lines;
    LyrTagParser parser(*this);

    bool isLrcFile = false;
    digest = 0xFFFFFFFF;
    lyrics.splitLines(lines);
    for (auto &line : lines) {
        if (parser.parseLrcTag(line)) {
            isLrcFile = true;
        } else if (!isLrcFile && parser.parseTxtTag(line)) {
            // txt format
        } else {
            digest = crc32(digest, (uint8_t *)line.data, line.len);
        }
    }

    digest = crc32(digest, (uint8_t *)artist.c_str(), (uint32_t)artist.size());
    digest = crc32(digest, (uint8_t *)album.c_str(), (uint32_t)album.size());
    digest = crc32(digest, (uint8_t *)title.c_str(), (uint32_t)title.size());
    digest = crc32(digest, (uint8_t *)mediaLength.c_str(), (uint32_t)mediaLength.size());

    lyrContentType = isLrcFile ? LCT_LRC : LCT_TXT;
}

#define ENCRYPT_INT_LEN        (sizeof(uint32_t)  + 1)
#define ENCRYPT_CRC_EXTRA      2

cstr_t __MLID_Table = { "qscavhuiklopnbzwe" };
uint8_t __MLStr_IDTable['z' - 'a' + 1] = { 0xFF };

string MLIdEncode(uint8_t byData[], int nLen) {
    string str;

    for (int i = 0; i < nLen; i++) {
        uint32_t c = byData[i];
        str.push_back(__MLID_Table[(c >> 4) & 0xF]);
        str.push_back(__MLID_Table[c & 0xF]);
    }

    return str;
}

string MLIdDecode(cstr_t szIn, int nLen) {
    if (nLen >= 3 && szIn[2] == '_') {
        szIn += 3;
        nLen -= 3;
    }

    if (__MLStr_IDTable[0] == 0xFF) {
        // NOT initialized, init it.
        for (int i = 0; i < 'z' - 'a' + 1; i++) {
            __MLStr_IDTable[0] = 0xFF;
        }

        for (int i = 0; i < 16; i++) {
            __MLStr_IDTable[__MLID_Table[i] - 'a'] = i;
        }
    }

    string ret;

    for (int i = 0; i < nLen; i += 2) {
        uint8_t c;
        c = __MLStr_IDTable[szIn[i] - 'a'] << 4;
        c |= __MLStr_IDTable[szIn[i + 1] - 'a'];
        ret.push_back((char)c);
    }

    return ret;
}

// uint8_t[0] CRC, followed 4 bytes are network uint8_t.
string encryptLyricsID(uint32_t id) {
    uint8_t buf[64];

    uint32ToLE(id, buf + 1);

    // Calculate CRC
    buf[0] = ENCRYPT_CRC_EXTRA;
    for (int i = 1; i < ENCRYPT_INT_LEN; i++) {
        buf[0] += buf[i];
    }

    int n = ENCRYPT_INT_LEN;
    while (n > 1 && buf[n - 1] == 0) {
        n--;
    }
    return MLIdEncode(buf, n);
}

int64_t decryptLyricsID(cstr_t textEncoded) {
    auto decoded = MLIdDecode(textEncoded, (int)strlen(textEncoded));
    decoded.resize(ENCRYPT_INT_LEN, 0);

    // Verify CRC
    uint8_t byCrc = ENCRYPT_CRC_EXTRA;
    for (int i = 1; i < ENCRYPT_INT_LEN; i++) {
        byCrc += (uint8_t)decoded[(uint32_t)i];
    }

    // CRC wrong
    if (byCrc != (uint8_t)decoded[(uint32_t)0]) {
        return -1;
    }

    return uint32FromLE((uint8_t*)decoded.c_str() + 1);
}
