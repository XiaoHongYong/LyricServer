#include "Types.h"
#include "LyricsServer.h"
#include "../../LyricsLib/LyricsKeywordFilter.h"
#include "UserDB.h"
#include "SpamLyricsFilter.h"
#include "DataSyncLog.hpp"
#include "apis/ClientApisHandler.hpp"
#include "apis/StatusHandler.hpp"
#include "apis/DatabaseApisHandler.hpp"

CProfile g_profile;
CLog g_log;

const int SAME_LYR_EXISTS_MAX = 8;

cstr_t GenRandLyrFilePrefix() {
    static char str[4];

    str[0] = 'a' + rand() % ('z' - 'a');
    str[1] = 'a' + rand() % ('z' - 'a');
    str[2] = '_';
    str[3] = '\0';

    return str;
};

void getRelatedDir(cstr_t szDir, cstr_t szBaseDir, char *szRelatedDir) {
    auto nBaseLen = strlen(szBaseDir);
    if (strncasecmp(szBaseDir, szDir, nBaseLen) == 0) {
        if (szDir[nBaseLen] == PATH_SEP_CHAR) {
            nBaseLen++;
        }
        strcpy(szRelatedDir, szDir + nBaseLen);
    } else {
        assert(0 && "GetRelatedPath()");
        strcpy(szRelatedDir, szDir);
    }
}

cstr_t GenRandLyrFilePrefix();

int iFindAtLineStart(const StringView &data, const StringView &pattern) {
    if (data.iStartsWith(pattern)) {
        return 0;
    }

    int start = 0;
    while (true) {
        start = data.strchr('\n', start);
        if (start == -1) {
            return -1;
        }
        start++;

        auto line = data.substr(start);
        if (line.iStartsWith(pattern)) {
            return start;
        }
    }
}

bool isWinNewLine(const StringView &data) {
    auto p = data.data, end = data.data + data.len;
    for (; p < end; p++) {
        if (*p == '\n') {
            return false;
        } else if (*p == '\r') {
            return true;
        }
    }

    return false;
}

string setLyricsId(const StringView &data, long lyricsId, bool isLrcTag) {
    static StringView LRC_ID("[id:");
    static StringView TXT_ID("id:");

    string id = encryptLyricsID(lyricsId);

    // int pos = -1, end = -1;
    int pos = iFindAtLineStart(data, isLrcTag ? LRC_ID : TXT_ID);
    if (pos != -1) {
        int end = -1;
        if (isLrcTag) {
            int end1 = data.strchr(']', pos);
            int end2 = data.strchr('\n', pos + 1);
            if (end1 != -1 && end1 < end2) {
                // 结束的 ']' 必须在此行内
                pos += LRC_ID.len;
                end = end1 + 1;
            }
        } else {
            end = data.strchr('\n', pos);
            if (end != -1) {
                pos += LRC_ID.len;
            }
        }

        if (end != -1) {
            string lyrics;

            lyrics.append(data.data, pos);
            lyrics.push_back(' ');
            lyrics.append(id);
            lyrics.append(data.data + end, data.len - end);

            return lyrics;
        }
    }

    string lyrics;
    if (isLrcTag) {
        // 添加到开头
        lyrics.append("[id: ");
        lyrics.append(id);
        lyrics.push_back(']');

        if (isWinNewLine(data)) {
            lyrics.append("\r\n");
        } else {
            lyrics.push_back('\n');
        }

        lyrics.append(data.data, data.len);
    } else {
        // 添加到结尾
        lyrics.append(data.data, data.len);
        if (isWinNewLine(data)) {
            lyrics.append("\r\n");
        } else {
            lyrics.push_back('\n');
        }

        lyrics.append("id: ");
        lyrics.append(id);
    }

    return lyrics;
}

uint32_t VersionNumMake(int nMajor, int nMinor, int nBuild) {
    assert(nMajor <= 0xFF && nMinor <= 0xFF && nBuild <= 0xFFFF);

    return MAKEINT(nBuild, MAKEWORD(nMinor, nMajor));
}

uint32_t versionNumParse(cstr_t szVersion)
{
    int nVerMajor, nVerMinor, nVerBuild;
    int ret = sscanf(szVersion, "%d.%d.%d", &nVerMajor, &nVerMinor, &nVerBuild);
    if (ret == 3) {
        return VersionNumMake(nVerMajor, nVerMinor, nVerBuild);
    }
    return 0;
}

class MLMsgRetActivateLicense : public MLRetMsg {
    void ToXMLAttribute(CXMLWriter &writer) {
        writer.writeAttribute("badrc", 0);
        writer.writeAttribute("ls_sd", "2019-10-01");
        writer.writeAttribute("ls_lu", 3);
        writer.writeAttribute("ls_dd", 0);
    }

};

//////////////////////////////////////////////////////////////////////////
// LyricsServer

LyricsServer::LyricsServer() : m_spamFilter("LyricsServer-") {
    srand((int)time(NULL));
}

LyricsServer::~LyricsServer() {
}

int LyricsServer::init(ServerConfig &config) {
    m_lyricsDir = config.lyricsDir;
    dirStringAddSep(m_lyricsDir);

    m_relatedUploadDir = config.uploadDirName;
    dirStringAddSep(m_relatedUploadDir);

    m_lyricsHttpLinkBase = config.lyricsHttpUrlBase;

    m_spamFilter.Load(dirStringJoin(config.rootDir, "data/").c_str());

    CLyricsKeywordFilter::init();

    if (!m_fpSyncLog.open(dirStringJoin(config.rootDir, "data-sync.log").c_str(), "a+b")) {
        LOG("Failed to open data-sync.log");
        return ERR_OPEN_FILE;
    }

    int ret = m_dbLyrics.init(config.rootDir);
    if (ret != ERR_OK) {
        LOG("Failed to init lyrics db");
        return ret;
    }

    auto fn = dirStringJoin(config.rootDir, "database/users.db");
    ret = m_dbUser.init(fn.c_str());
    if (ret != ERR_OK) {
        LOG("Failed to init users db");
    }

    CMLPacketAccountMgr *pMgr = CMLPacketAccountMgr::getInstance();
    pMgr->addAccount(4, "Mlv1clt4.0");
    pMgr->m_packFlag = MPV_V1_MD5_ID;

    HttpServer::init(config.address, config.port);

    registerRequestHandler(std::make_shared<StatusHandler>(this));
    registerRequestHandler(std::make_shared<ClientApisHandler>(this));
    registerRequestHandler(std::make_shared<DatabaseApisHandler>(m_dbUser.db(), "/db-api/user"));
    registerRequestHandler(std::make_shared<DatabaseApisHandler>(m_dbLyrics.db(), "/db-api/lyrics"));

    return ERR_OK;
}

int LyricsServer::processBatchSearchCmd(MLMsgCmdBatchSearch &cmdSearch, MLMsgRetBatchSearch &retSearch) {
    CColonSeparatedValues csv;
    int i = 0;
    for (MLListSearchItems::iterator it = cmdSearch.listSearchItems.begin();
    it != cmdSearch.listSearchItems.end() && i < 50; ++it, ++i)
        {
        MLSearchItem &item = *it;
        MLLyricsInfoLite infoLite;

        int ret = searchBestMatchLyrics(item.strArtist.c_str(), item.strAlbum.c_str(),
            item.strTitle.c_str(), item.nMediaLength, infoLite);

        if (ret != ERR_OK) {
            infoLite.strFile.clear();
        }
        retSearch.listLyricsInfo.push_back(infoLite);

        // SearchRet Code, Time cost, artist, title.
        csv.clear();
        csv.addValue(item.strArtist.c_str());
        csv.addValue(item.strTitle.c_str());
    }

    assert(retSearch.listLyricsInfo.size() == cmdSearch.listSearchItems.size());

    return ERR_OK;
}

int LyricsServer::searchMatchedLyricsOnly(cstr_t szArCmp, cstr_t szTiCmp, RetLyrInfoList &listLyr) {
    int ret = m_dbLyrics.SearchLyricsByArtistTitle(szArCmp, szTiCmp, listLyr);
    if (ret != ERR_OK) {
        return ret;
    }

    if (listLyr.empty()) {
        return ERR_NOT_FOUND;
    }

    return ERR_OK;
}

int LyricsServer::searchBestMatchLyrics(cstr_t szArtist, cstr_t szAlbum, cstr_t szTitle, int nMediaLength, MLLyricsInfoLite &infoLite) {
    int ret = ERR_OK;
    string strArtistCmp;
    string strAlbumCmp;
    string strTitleCmp;

    CLyricsKeywordFilter::filter(szArtist, strArtistCmp);
    CLyricsKeywordFilter::filter(szAlbum, strAlbumCmp);
    CLyricsKeywordFilter::filter(szTitle, strTitleCmp);

    RetLyrInfoList vLyrics;

    ret = m_dbLyrics.SearchLyricsByArtistTitle(strArtistCmp.c_str(),
        strTitleCmp.c_str(), vLyrics);
    if (ret != ERR_OK) {
        return ret;
    }

    RetLyrInfo *bestMatch = NULL;
    float nBestMatchValue = 0.0;
    string strAlbumCmp2;

    for (auto it = vLyrics.begin(); it != vLyrics.end(); ++it) {
        RetLyrInfo &info = *it;
        float nMatchValue = 60;
        if (info.contentType == LCT_TXT) {
            nMatchValue = 60;
        } else if (info.contentType == LCT_LRC) {
            nMatchValue = 70;
        }

        // increase rating
        if (info.nRateCount > 0) {
            float fRate = info.fRate;
            nMatchValue += fRate - 3;
            if (info.nRateCount > 5) {
                if (fRate >= 4.8) {
                    nMatchValue++;
                } else if (fRate <= 3.0) {
                    nMatchValue--;
                }
            }
            nMatchValue += info.nRateCount / (float)1000.0;
        }

        // Media Length equal?
        if (nMediaLength > 0) {
            if (info.nTimeLength == nMediaLength) {
                nMatchValue += 2.0;
            } else if (abs(info.nTimeLength - nMediaLength) <= 3) {
                nMatchValue += 1.0;
            }
        }

        // Album equal?
        CLyricsKeywordFilter::filter(info.strAlbum.c_str(), strAlbumCmp2);
        if (strcmp(strAlbumCmp2.c_str(), strAlbumCmp.c_str()) == 0) {
            nMatchValue += 1.0;
        }

        if (nMatchValue > nBestMatchValue) {
            nBestMatchValue = nMatchValue;
            bestMatch = &info;
        }
    }

    if (!bestMatch) {
        return ERR_NOT_FOUND;
    }

    infoLite.strFile = m_lyricsDir;
    infoLite.strFile += bestMatch->strLink;
    strrep(infoLite.strFile, '/', '\\');

    // Set the file name to client, it used to save as file name.
    infoLite.strSaveName = bestMatch->strArtist;
    if (infoLite.strSaveName.size()) {
        infoLite.strSaveName += " - ";
    }
    infoLite.strSaveName += bestMatch->strTitle;
    infoLite.strSaveName = fileNameFilterInvalidChars(infoLite.strSaveName.c_str());
    infoLite.strSaveName += fileGetExt(bestMatch->strLink.c_str());

    return ERR_OK;
}

void LyricsServer::process(uint8_t *data, size_t len, HttpResponse &response) {
    string buf;
    buf.append((char *)data, len);

    int ret = m_packetWrapper.unwrapp(buf);
    if (ret != ERR_OK) {
        response.statusCode = HttpStatusCode::BAD_REQUEST;
        response.body = "400 Bad Request";
        response.sendAll();
        return;
    }

    CSimpleXML xml;
    if (!xml.parseData(buf.data(), buf.size()) && strcasecmp(xml.m_encoding.c_str(), SZ_UTF8) != 0) {
        response.statusCode = HttpStatusCode::BAD_REQUEST;
        response.body = "400 Bad Request";
        response.sendAll();
        return;
    }

    CMLBinXMLWriter xmlStream;

    MLMsgCmd msgCmd = CMLProtocol::getMsgCommand(xml);
    switch (msgCmd) {
    case MC_LOGIN: {
        MLMsgCmdLogin cmd;
        MLMsgRetLogin retMsg;

        retMsg.strOrgCmd = xml.m_pRoot->name;
        if (cmd.fromXML(xml.m_pRoot) == ERR_OK) {
            long nUploaderId;
            retMsg.result = m_dbUser.LoginUser(cmd.name.c_str(), cmd.strPwd.c_str(), nUploaderId);
            retMsg.toXML(xmlStream);
        }
        break;
    }
    case MC_SEARCH_V0:
        // 不再支持旧的消息
        break;
    case MC_SEARCH: {
        MLMsgCmdSearch cmd;
        MLMsgRetSearch retMsg;

        retMsg.strOrgCmd = xml.m_pRoot->name;
        if (cmd.fromXML(xml.m_pRoot) == ERR_OK) {
            retMsg.result = processSearchCmd(cmd, retMsg);
            retMsg.toXML(xmlStream);
        }
        break;
    }
    case MC_BATCH_SEARCH: {
        MLMsgCmdBatchSearch cmd;
        MLMsgRetBatchSearch retMsg;

        retMsg.strOrgCmd = xml.m_pRoot->name;
        if (cmd.fromXML(xml.m_pRoot) == ERR_OK) {
            retMsg.result = processBatchSearchCmd(cmd, retMsg);
            retMsg.toXML(xmlStream);
        }
        break;
    }
    case MC_UPLOAD:{
        MLMsgCmdUpload cmd;
        MLMsgRetUpload retMsg;

        retMsg.strOrgCmd = xml.m_pRoot->name;
        if (cmd.fromXML(xml.m_pRoot) == ERR_OK) {
            retMsg.result = processUploadCmd(cmd, retMsg);
            retMsg.toXML(xmlStream);
        }
        break;
    }
    case MC_ACTIVATE: {
        MLMsgRetActivateLicense retMsg;
        retMsg.result = ERR_OK;
        retMsg.toXML(xmlStream);
        break;
    }
    default:
        LOG("unsupported command(%d) from client", msgCmd);
        break;
    }

    if (xmlStream.isEmpty()) {
        MLRetMsg retMsg;
        retMsg.result = ERR_BAD_MSG;
        retMsg.strMessage = "Bad command, not supported.";
        retMsg.toXML(xmlStream);
    }

    response.body = xmlStream.getBuffer();
    m_packetWrapper.wrapp(response.body);

    response.statusCode = HttpStatusCode::OK;
    response.sendAll();
}

int LyricsServer::processSearchCmd(MLMsgCmdSearch &cmdSearch, MLMsgRetSearch &retSearch) {
    retSearch.strServerUrl = m_lyricsHttpLinkBase;

    if (cmdSearch.strArtist.empty() && cmdSearch.strTitle.empty()) {
        return ERR_CMD_PARAM;
    }

    string strArCmp, strTiCmp;
    CLyricsKeywordFilter::filter(cmdSearch.strArtist.c_str(), strArCmp);
    CLyricsKeywordFilter::filter(cmdSearch.strTitle.c_str(), strTiCmp);

    if (strArCmp.size() && strTiCmp.size()) {
        // Only return matched lyrics? Artist and title are both match
        if (searchMatchedLyricsOnly(strArCmp.c_str(), strTiCmp.c_str(), retSearch.listResultFiles) == ERR_OK) {
            return ERR_OK;
        }
    }

    int ret;
    if (strTiCmp.empty()) {
        // search by artist
        ret = m_dbLyrics.SearchLyricsByArtist(strArCmp.c_str(), retSearch.listResultFiles);
    } else {
        // search by title
        ret = m_dbLyrics.SearchLyricsByTitle(strTiCmp.c_str(), retSearch.listResultFiles);
    }

    if (retSearch.listResultFiles.empty()) {
        ret = ERR_NOT_FOUND;
    } else {
        ret = ERR_OK;
    }

    return ret;
}

int LyricsServer::processUploadCmd(MLMsgCmdUpload &cmdUpload, MLMsgRetUpload &retMsg) {
    // Get user ID
    long nUploaderId = 0;
    int ret = m_dbUser.LoginUser(cmdUpload.strLoginName.c_str(), cmdUpload.strPwdMask.c_str(), nUploaderId);
    if (ret != ERR_OK) {
        return ret;
    }

    string &strLyrContent = cmdUpload.strFileContent;
    auto szUploader = cmdUpload.strLoginName.c_str();
    LyricsInfo props;

    props.parse(strLyrContent);
    if (props.title.empty()) {
        return ERR_LYR_NO_TAG_FOUND;
    }

    {
        // 检查歌词内容
        int nLevel = 0;
        if (m_spamFilter.IsNameFiltered(props.artist.c_str(), props.album.c_str(),
                props.title.c_str(), nLevel, &retMsg.strMessage)) {
            return ERR_BAD_FILE_CONTENT;
        }
        if (m_spamFilter.IsContentFiltered(props.lyrContentType, strLyrContent.c_str(),
            (int)strLyrContent.size(), nLevel, &retMsg.strMessage)) {
            return ERR_BAD_FILE_CONTENT;
        }
    }

    if (!props.id.empty()) {
        long nOrgLyricsId = decryptLyricsID(props.id.c_str());
        if (nOrgLyricsId != 0) {
            // Lyrics ID exist, check for updating.
            RetLyrInfo lyrInfo;
            int ret = m_dbLyrics.SearchLyricsByArtistTitleID(props.arCmp.c_str(), props.tiCmp.c_str(), nOrgLyricsId, lyrInfo);
            if (ret == ERR_OK && lyrInfo.contentType == props.lyrContentType) {

                uint32_t digestExisting = 0;
                int ret = m_dbLyrics.getLyricsDigest(lyrInfo.lyricsID, digestExisting);
                if (ret != ERR_OK) {
                    return ret;
                }

                if (digestExisting == props.digest) {
                    retMsg.result = ERR_UPLOAD_EXIST;
                    // retMsg.strMessage = "Lyrics exists already.";
                    return retMsg.result;
                }

                retMsg.strLyricsId = encryptLyricsID(nOrgLyricsId);
                if (lyrInfo.uploaderID == nUploaderId) {
                    // Update lyrics.
                    props.relatedHttpLink = lyrInfo.strLink;
                    props.lyricsID = lyrInfo.lyricsID;
                    return updateLyrics(props, strLyrContent);
                }
            }
        }
    }

    //
    // This is a new upload of lyrics.
    //
    props.uploaderId = nUploaderId;
    props.by = szUploader;

    ret = addNewLyrics(props, strLyrContent);
    if (ret != ERR_OK) {
        return ret;
    }

    retMsg.strLyricsId = encryptLyricsID((uint32_t)props.lyricsID);

    return ERR_OK;
}

// New uploaded lyrics.
int LyricsServer::addNewLyrics(LyricsInfo &lyrInfo, string &strLyrContent) {
    // Add lyrics in DB first
    int nRet = m_dbLyrics.AddLyrics(lyrInfo);
    if (nRet != ERR_OK) {
        return nRet;
    }
    assert(lyrInfo.lyricsID != 0);

    strLyrContent = setLyricsId(strLyrContent, lyrInfo.lyricsID, lyrInfo.lyrContentType >= LCT_LRC);

    // Save lyrics file
    nRet = saveLyricsFile(lyrInfo, strLyrContent);
    if (nRet != ERR_OK) {
        return nRet;
    }

    dslWriteLyricsFile(m_fpSyncLog, strLyrContent, lyrInfo.relatedHttpLink, DSA_CREATE);
    dslWriteDbLyrics(m_fpSyncLog, lyrInfo, DSA_CREATE);

    return m_dbLyrics.UpdateLyricsLinkById(lyrInfo.lyricsID, lyrInfo.relatedHttpLink.c_str());
}

int LyricsServer::updateLyrics(LyricsInfo &lyrInfo, string &strLyrContent) {
    strLyrContent = setLyricsId(strLyrContent, lyrInfo.lyricsID, lyrInfo.lyrContentType >= LCT_LRC);

    string strFile = m_lyricsDir + lyrInfo.relatedHttpLink;
    if (!writeFile(strFile.c_str(), strLyrContent)) {
        return ERR_OPEN_FILE;
    }

    int ret = m_dbLyrics.UpdateLyricsPropById(lyrInfo.lyricsID, lyrInfo);
    if (ret != ERR_OK) {
        return ret;
    }

    dslWriteLyricsFile(m_fpSyncLog, strLyrContent, lyrInfo.relatedHttpLink, DSA_UPDATE);
    dslWriteDbLyrics(m_fpSyncLog, lyrInfo, DSA_UPDATE);

    return ERR_OK;
}

int LyricsServer::saveLyricsFile(LyricsInfo &lyrInfo, string &strLyrContentUtf8) {
    string strFile = m_lyricsDir + m_relatedUploadDir;

    {
        time_t now = time(NULL);
        tm *t = localtime(&now);

        char szTemp[64];
        sprintf(szTemp, "%02d%02d%02d/", t->tm_year + 1900 - 2000, t->tm_mon + 1, t->tm_mday);
        strFile += szTemp;

        if (!isDirExist(strFile.c_str())) {
            createDirectoryAll(strFile.c_str());
        }
    }

    strFile += GenRandLyrFilePrefix();
    strFile += encryptLyricsID(lyrInfo.lyricsID);
    strFile += lyrInfo.lyrContentType == LCT_TXT ? ".txt" : ".lrc";

    // Update file link in lyrics db.
    char szRelatedLink[MAX_PATH];
    getRelatedDir(strFile.c_str(), m_lyricsDir.c_str(), szRelatedLink);
    strrep(szRelatedLink, '\\', '/');
    lyrInfo.relatedHttpLink = szRelatedLink;

    if (!writeFile(strFile.c_str(), strLyrContentUtf8)) {
        return ERR_OPEN_FILE;
    }

    return ERR_OK;
}
