#include "Types.h"
#include "SpamLyricsFilter.h"
#include "../../TinyJS/utils/FileApi.h"
#include "../../TinyJS/utils/StringEx.h"


bool TextFileToVStr(cstr_t szFile, vector<string> &vStr)
{
    FILE    *fp;
    char    szBuff[1024];

    fp = fopen(szFile, "rt");
    if (!fp)
        return false;

    while (fgets(szBuff, CountOf(szBuff) - 1, fp))
    {
        int        nLen;
        nLen = strlen(szBuff);
        if (nLen > 0)
        {
            while (szBuff[nLen - 1] == '\r'
                || szBuff[nLen - 1] == '\n')
                nLen--;
            if (nLen > 0)
            {
                szBuff[nLen] = '\0';
                vStr.push_back(szBuff);
            }
        }
    }
    fclose(fp);

    return true;
}

bool ToFilterItem(vector<string> &vStr, SpamLyricsFilter::CVFilterA &vFilter) {
    int nLevel;
    cstr_t str;
    for (int i = 0; i < (int)vStr.size(); i++) {
        nLevel = atoi(vStr[i].c_str());
        str = strstr(vStr[i].c_str(), ", ");
        if (!str) {
            return false;
        }
        vFilter.Add(str + 2, nLevel);
    }

    return true;
}


template<class XCHAR>
void GetLineInfo(const XCHAR *szText, int &nLineCount, int &nMaxWdith) {
    const XCHAR *szFindPos;
    int nCur = 0;

    nLineCount = 1;
    nMaxWdith = 0;

    szFindPos = szText;

    while (*szFindPos) {
        if (*szFindPos == '\n') {
            if (nCur > nMaxWdith) {
                nMaxWdith = nCur;
            }
            nCur = 0;
            nLineCount++;
        } else {
            nCur++;
        }
        szFindPos++;
    }
    if (nCur > nMaxWdith) {
        nMaxWdith = nCur;
    }
}

template<class XCHAR>
void GetLrcTimeTagCount(const XCHAR *szText, int &nCount) {
    assert(szText);

    const XCHAR *szPos;

    szPos = szText;
    nCount = 0;
    while (*szPos != '\0') {
        // [12:23.22]
        if (*szPos != '[') {
            szPos++;
            continue;
        }
        szPos++;

        // 12:23.22]
        if (!isDigit(*szPos)) {
            continue;
        }
        szPos++;

        while (isDigit(*szPos)) {
            szPos++;
        }

        // :23.22]
        if (*szPos != ':' && *szPos != '.') {
            continue;
        }
        szPos++;

        // 23.22]
        if (!isDigit(*szPos)) {
            continue;
        }
        szPos++;

        while (isDigit(*szPos)) {
            szPos++;
        }

        if (*szPos == ']') {
            // ]
            szPos++;
            nCount++;
            continue;
        }

        // .22]
        if (*szPos != '.' && *szPos != ':') {
            continue;
        }
        szPos++;

        // 22]
        if (!isDigit(*szPos)) {
            continue;
        }
        szPos++;

        while (isDigit(*szPos)) {
            szPos++;
        }

        // ]
        if (*szPos == ']') {
            szPos++;
            nCount++;
            continue;
        }
    }
}


SpamLyricsFilter::SpamLyricsFilter(cstr_t szCfgFilePrefix) {
    strCfgFilePrefix = szCfgFilePrefix;
}

SpamLyricsFilter::~SpamLyricsFilter() {

}

bool SpamLyricsFilter::Load(cstr_t dataDir) {
    string strFile;
    vector<string> vStr;

    m_vContentFilter.clear();

    strFile = dirStringJoin(dataDir, strCfgFilePrefix + "SpamNameFilter.txt");
    if (!readFile(strFile.c_str(), m_strNameFilter)) {
        LOG("Can't Open Filter File: %s", strFile.c_str());
        return false;
    }

    TextFileToVStr(strFile.c_str(), vStr);
    ToFilterItem(vStr, m_vNameFilter);
    //
    //     m_vNameFilter.Add("www.");
    //     m_vNameFilter.Add(".net");
    //     m_vNameFilter.Add(".com");

    strFile = dirStringJoin(dataDir, strCfgFilePrefix + "SpamContentFilter.txt");
    if (!readFile(strFile.c_str(), m_strContentFilter)) {
        LOG("Can't Open Filter File: %s", strFile.c_str());
        return false;
    }

    vStr.clear();
    TextFileToVStr(strFile.c_str(), vStr);
    ToFilterItem(vStr, m_vContentFilter);

    return true;
}

cstr_t SpamLyricsFilter::GetNameFilter() {
    return m_strNameFilter.c_str();
}

void SpamLyricsFilter::SetMinContentSize(int nSize) {
    m_nMinContentSize = nSize;
}

bool SpamLyricsFilter::IsNameFiltered(const StringView &artist, const StringView &album, const StringView &title, int &nLevel, string *pstrReason) {
    if (title.len == 0) {
        if (pstrReason) {
            *pstrReason = "Title should NOT be empty.";
        }
        nLevel = SF_ABS_SPAM;
        return true;
    }

    for (int i = 0; i < (int)m_vNameFilter.size(); i++) {
        ITEMA &item = m_vNameFilter[i];
        if (artist.stristr(item.str) != -1) {
            if (pstrReason) {
                *pstrReason = "Artist should not contain these text: " + item.str;
            }
            nLevel = item.nLevel;
            return true;
        }

        if (album.stristr(item.str) != -1) {
            if (pstrReason) {
                *pstrReason = "Album should not contain these text: " + item.str;
            }
            nLevel = item.nLevel;
            return true;
        }

        if (title.stristr(item.str) != -1) {
            if (pstrReason) {
                *pstrReason = "Title should not contain these text: " + item.str;
            }
            nLevel = item.nLevel;
            return true;
        }
    }

    return false;
}

bool SpamLyricsFilter::IsContentFiltered(LyricsContentType lyrContentType, cstr_t szContent, int nLen, int &nLevel, string *pstrReason) {
    if (m_strFilterExcept.size()) {
        if (stristr(szContent, m_strFilterExcept.c_str())) {
            return false;
        }
    }

    // filter by content length
    if (nLen < m_nMinContentSize) {
        if (pstrReason) {
            *pstrReason = stringPrintf("Lyrics content length: %d is less than %d", nLen, m_nMinContentSize).c_str();
        }
        nLevel = SF_FILE_SIZE;
        return true;
    }

    if (nLen > 1024 * 200) {
        if (pstrReason) {
            *pstrReason = stringPrintf("Lyrics content length: %d is larger than 200KB", nLen).c_str();
        }
        nLevel = SF_FILE_SIZE;
        return true;
    }

    for (int i = 0; i < (int)m_vContentFilter.size(); i++) {
        if (stristr(szContent, m_vContentFilter[i].str.c_str())) {
            if (pstrReason) {
                *pstrReason = "Lyrics content should not contain these text: " + m_vContentFilter[i].str;
            }
            nLevel = m_vContentFilter[i].nLevel;
            return true;
        }
    }

    if (lyrContentType == LCT_LRC) {
        // the lrc file hasn't any synchronized time tag.
        int nTimeTagaccount;

        GetLrcTimeTagCount(szContent, nTimeTagaccount);

        if (nTimeTagaccount < 3) {
            // time tag is less than 3
            if (pstrReason) {
                *pstrReason = stringPrintf("The lrc file is fake, only %d time tag defined.", nTimeTagaccount).c_str();
            }
            nLevel = SF_FAKE_LRC;
            return true;
        }
    } else {
        // Is the file a lrc?
        int nTimeTagaccount;

        GetLrcTimeTagCount(szContent, nTimeTagaccount);

        if (nTimeTagaccount > 0) {
            if (pstrReason) {
                *pstrReason = stringPrintf("The txt file is a lrc file, %d time tag defined.", nTimeTagaccount).c_str();
            }
            nLevel = SF_TXT_TO_LRC;
            return true;
        }
    }

    if (lyrContentType == LCT_TXT) {
        // Filter by lines, only for lrc files.
        int nLineCount, nMaxWdith;

        GetLineInfo(szContent, nLineCount, nMaxWdith);

        if (nLineCount < m_nMinContentLines) {
            if (pstrReason) {
                *pstrReason = stringPrintf("The count of lyrics lines: %d is less than %d", nLineCount, m_nMinContentLines).c_str();
            }
            nLevel = SF_FILE_LINES_TOO_LESS;
            return true;
        } else if (nMaxWdith >= 130 && nMaxWdith > nLen / nLineCount * 2) {
            if (pstrReason) {
                *pstrReason = stringPrintf("One or more lyrics lines are too long, please format them well.").c_str();
            }
            nLevel = SF_FILE_LINES_TOO_LONG;
            return true;
        }
    }

    return false;
}
