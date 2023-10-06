#pragma once

#ifndef ServerNgx_src_SpamLyricsFilter_h
#define ServerNgx_src_SpamLyricsFilter_h

#include "../../MediaTags/LyricsData.h"


class SpamLyricsFilter {
public:
    enum {
        SF_ABS_SPAM                 = 1,
        SF_HAS_SPAM_INFO            = 2,
        SF_FILE_SIZE                = 3,
        SF_FILE_LINES_TOO_LESS      = 4,
        SF_FILE_LINES_TOO_LONG      = 5,
        SF_FAKE_LRC                 = 6,
        SF_NAME_ONE_CHAR            = 7,
        SF_TXT_TO_LRC               = 8,
    };

    SpamLyricsFilter(cstr_t szCfgFilePrefix);
    virtual ~SpamLyricsFilter();

    struct ITEMA {
        int                         nLevel;
        string                      str;
    };

    class CVFilterA : public vector<ITEMA> {
    public:
        void Add(cstr_t szStr, int nLevel) {
            for (iterator it = begin(); it != end(); it++) {
                ITEMA &item = *it;
                if (stristr(item.str.c_str(), szStr)) {
                    erase(it);
                    ITEMA x;
                    x.nLevel = nLevel;
                    x.str = szStr;
                    push_back(x);
                    return;
                }
            }
            ITEMA x;
            x.nLevel = nLevel;
            x.str = szStr;
            push_back(x);
        }
    };

    bool Load(cstr_t dataDir);

    cstr_t GetNameFilter();

    int GetMinContentSize() { return m_nMinContentSize; }
    void SetMinContentSize(int nSize);

    bool IsNameFiltered(const StringView &artist, const StringView &album, const StringView &title, int &nLevel, string *pstrReason);

    bool UseContentFilter() { return !m_vContentFilter.empty(); }

    bool IsContentFiltered(LyricsContentType lyrContentType, cstr_t szContent, int nLen, int &nLevel, string *pstrReason);

protected:
    string                      strCfgFilePrefix;

    string                      m_strFilterExcept;
    int                         m_nMinContentLines = 7;
    int                         m_nMinContentSize = 200;
    string                      m_strNameFilter, m_strContentFilter;
    CVFilterA                   m_vNameFilter;
    CVFilterA                   m_vContentFilter;

};

#endif // !defined(ServerNgx_src_SpamLyricsFilter_h)
