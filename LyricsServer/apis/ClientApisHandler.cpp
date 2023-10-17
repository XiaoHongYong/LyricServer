//
//  ClientApisHandler.cpp
//

#include "ClientApisHandler.hpp"
#include "LyricsLib/LyricsKeywordFilter.h"


uint32_t VersionNumMake(int nMajor, int nMinor, int nBuild) {
    assert(nMajor <= 0xFF && nMinor <= 0xFF && nBuild <= 0xFFFF);

    return MAKEINT(nBuild, MAKEWORD(nMinor, nMajor));
}

uint32_t versionNumParse(cstr_t szVersion) {
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

ClientApisHandler::ClientApisHandler(LyricsServer *server) : _server(server), _dbLyrics(server->_dbLyrics), _dbUser(server->_dbUser), _spamFilter("LyricsServer-") {

    CMLPacketAccountMgr *pMgr = CMLPacketAccountMgr::getInstance();
    pMgr->addAccount(4, "Mlv1clt4.0");
    pMgr->m_packFlag = MPV_V1_MD5_ID;

    _spamFilter.Load(dirStringJoin(g_conf.rootDir, "data/").c_str());
}

const string &ClientApisHandler::getUriPath() const {
    static string path = "/searchlyrics.htm";
    return path;
}

int ClientApisHandler::onRequestHeader(HttpConnectionPtr connection) {
    if (connection->request().methodID != HttpMethod::METHOD_POST) {
        auto &response = connection->response();
        response.statusCode = HttpStatusCode::NOT_FOUND;
        response.body = "404 NOT-FOUND-CLIENT";
        response.sendAll();
    }

    return ERR_OK;
}

int ClientApisHandler::onRequestBody(HttpConnectionPtr connection) {
    int ret = process(connection->request().body, connection->response());
    if (ret != ERR_OK) {
        _countBadMessages++;
        auto &response = connection->response();
        response.statusCode = HttpStatusCode::BAD_REQUEST;
        response.body = "400 Bad Request";
        response.sendAll();
    }

    return ERR_OK;
}

int ClientApisHandler::process(const string &data, HttpResponse &response) {
    string buf = data;

    int ret = _packetWrapper.unwrapp(buf);
    if (ret != ERR_OK) {
        return ERR_BAD_MSG;
    }

    CSimpleXML xml;
    if (!xml.parseData(buf.data(), buf.size()) && strcasecmp(xml.m_encoding.c_str(), SZ_UTF8) != 0) {
        return ERR_BAD_MSG;
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
            retMsg.result = _dbUser.LoginUser(cmd.name.c_str(), cmd.strPwd.c_str(), nUploaderId);
            retMsg.toXML(xmlStream);
            _countLogin++;
        }
        break;
    }
    case MC_SEARCH: {
        MLMsgCmdSearch cmd;
        MLMsgRetSearch retMsg;

        retMsg.strOrgCmd = xml.m_pRoot->name;
        if (cmd.fromXML(xml.m_pRoot) == ERR_OK) {
            _countSearch++;
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
            _countBatchSearch++;
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
            _countUpload++;
            retMsg.result = processUploadCmd(cmd, retMsg);
            retMsg.toXML(xmlStream);
        }
        break;
    }
    case MC_CAN_UPLOAD:
    case MC_UPLOADV0:
    case MC_SEARCH_V0:
        // 不再支持旧的消息
        _countOldMessages++;
        // No break, continue executing...
    case MC_ACTIVATE: {
        MLMsgRetActivateLicense retMsg;
        retMsg.result = ERR_OK;
        retMsg.toXML(xmlStream);
        _countOldMessages++;
        break;
    }
    default:
        LOG(INFO) << "unsupported command from client: " << (int)msgCmd;
        return ERR_BAD_MSG;
    }

    if (xmlStream.isEmpty()) {
        _countBadMessages++;
        return ERR_BAD_MSG;
    }

    response.body = xmlStream.getBuffer();
    _packetWrapper.wrapp(response.body);

    response.statusCode = HttpStatusCode::OK;
    response.sendAll();

    return ERR_OK;
}

int ClientApisHandler::processBatchSearchCmd(MLMsgCmdBatchSearch &cmdSearch, MLMsgRetBatchSearch &retSearch) {
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

int ClientApisHandler::searchMatchedLyricsOnly(cstr_t szArCmp, cstr_t szTiCmp, RetLyrInfoList &listLyr) {
    int ret = _dbLyrics.SearchLyricsByArtistTitle(szArCmp, szTiCmp, listLyr);
    if (ret != ERR_OK) {
        return ret;
    }

    if (listLyr.empty()) {
        return ERR_NOT_FOUND;
    }

    return ERR_OK;
}

int ClientApisHandler::searchBestMatchLyrics(cstr_t szArtist, cstr_t szAlbum, cstr_t szTitle, int nMediaLength, MLLyricsInfoLite &infoLite) {
    int ret = ERR_OK;
    string strArtistCmp;
    string strAlbumCmp;
    string strTitleCmp;

    CLyricsKeywordFilter::filter(szArtist, strArtistCmp);
    CLyricsKeywordFilter::filter(szAlbum, strAlbumCmp);
    CLyricsKeywordFilter::filter(szTitle, strTitleCmp);

    RetLyrInfoList vLyrics;

    ret = _dbLyrics.SearchLyricsByArtistTitle(strArtistCmp.c_str(),
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

    infoLite.strFile = g_conf.lyricsDir;
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

int ClientApisHandler::processSearchCmd(MLMsgCmdSearch &cmdSearch, MLMsgRetSearch &retSearch) {
    retSearch.strServerUrl = g_conf.lyricsHttpUrlBase;

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
        ret = _dbLyrics.SearchLyricsByArtist(strArCmp.c_str(), retSearch.listResultFiles);
    } else {
        // search by title
        ret = _dbLyrics.SearchLyricsByTitle(strTiCmp.c_str(), retSearch.listResultFiles);
    }

    if (retSearch.listResultFiles.empty()) {
        ret = ERR_NOT_FOUND;
        _countSearchNotFound++;
    } else {
        ret = ERR_OK;
    }

    return ret;
}

int ClientApisHandler::processUploadCmd(MLMsgCmdUpload &cmdUpload, MLMsgRetUpload &retMsg) {
    if (!g_conf.isMaster) {
        LOG(ERROR) << "This server is NOT master, not support upload lyrics.";
        retMsg.strMessage = "This server do NOT allow upload lyrics, please contact us to report the error.";
        return ERR_NOT_AUTHORIZED;
    }

    // Get user ID
    long nUploaderId = 0;
    int ret = _dbUser.LoginUser(cmdUpload.strLoginName.c_str(), cmdUpload.strPwdMask.c_str(), nUploaderId);
    if (ret != ERR_OK) {
        LOG(INFO) << "Sign in fialed, name: " << cmdUpload.strLoginName << ", passwordMask: " << cmdUpload.strPwdMask;
        return ERR_BAD_USERPWD;
    }

    string &strLyrContent = cmdUpload.strFileContent;
    auto szUploader = cmdUpload.strLoginName.c_str();
    LyricsInfo props;

    props.parse(strLyrContent);
    if (props.title.empty()) {
        _countUploadFailed++;
        return ERR_LYR_NO_TAG_FOUND;
    }

    {
        // 检查歌词内容
        int nLevel = 0;
        if (_spamFilter.IsNameFiltered(props.artist.c_str(), props.album.c_str(),
                props.title.c_str(), nLevel, &retMsg.strMessage)) {
            LOG(INFO) << "Failed to upload, one of bad file artist: " << props.artist << ", album: " << props.album << ", title: " << props.title;
            _countUploadFailed++;
            return ERR_BAD_FILE_CONTENT;
        }
        if (_spamFilter.IsContentFiltered(props.lyrContentType, strLyrContent.c_str(),
            (int)strLyrContent.size(), nLevel, &retMsg.strMessage)) {
            LOG(INFO) << "Failed to upload, size: " << strLyrContent.size() << ", bad content: "  << strLyrContent.substr(0, 200);
            _countUploadFailed++;
            return ERR_BAD_FILE_CONTENT;
        }
    }

    ret = _dbLyrics.isLyricsExist(props.arCmp.c_str(), props.tiCmp.c_str(), props.digest);
    if (ret == ERR_OK) {
        retMsg.result = ERR_UPLOAD_EXIST;
        _countUploadExists++;
        LOG(INFO) << "Failed to upload, lyrics exists already.";
        return retMsg.result;
    }

    if (!props.id.empty()) {
        long nOrgLyricsId = decryptLyricsID(props.id.c_str());
        if (nOrgLyricsId != 0) {
            // Lyrics ID exist, check for updating.
            RetLyrInfo lyrInfo;
            int ret = _dbLyrics.SearchLyricsByArtistTitleID(props.arCmp.c_str(), props.tiCmp.c_str(), nOrgLyricsId, lyrInfo);
            if (ret == ERR_OK && lyrInfo.contentType == props.lyrContentType) {
                retMsg.strLyricsId = encryptLyricsID(nOrgLyricsId);
                if (lyrInfo.uploaderID == nUploaderId) {
                    // Update lyrics.
                    props.relatedHttpLink = lyrInfo.strLink;
                    props.lyricsID = lyrInfo.lyricsID;
                    return _server->updateLyrics(props, strLyrContent);
                }
            }
        }
    }

    //
    // This is a new upload of lyrics.
    //
    props.uploaderId = nUploaderId;
    props.by = szUploader;

    ret = _server->addNewLyrics(props, strLyrContent);
    if (ret != ERR_OK) {
        return ret;
    }

    retMsg.strLyricsId = encryptLyricsID((uint32_t)props.lyricsID);

    return ERR_OK;
}

void ClientApisHandler::dumpStatus(IJsonWriter *writer) {
    writer->startObject();
    writer->writePropInt64("CountRequests", _countTotal);
    writer->endObject();
}

void ClientApisHandler::dumpStatus(StatusLog &log) {
    log.writeInt("count-search", _countSearch);
    log.writeInt("count-search-not-found", _countSearchNotFound);
    log.writeInt("count-batch-search", _countBatchSearch);
    log.writeInt("count-login", _countLogin);
    log.writeInt("count-upload", _countUpload);
    log.writeInt("count-upload-exists", _countUploadExists);
    log.writeInt("count-upload-failed", _countUploadFailed);
    log.writeInt("count-old-messages", _countOldMessages);
    log.writeInt("count-bad-messages", _countBadMessages);

    _countSearch = 0;
    _countSearchNotFound = 0;
    _countBatchSearch = 0;
    _countLogin = 0;
    _countUpload = 0;
    _countUploadExists = 0;
    _countUploadFailed = 0;
    _countOldMessages = 0;
    _countBadMessages = 0;
}
