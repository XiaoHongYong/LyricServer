#include "Types.h"
#include "LyricsServer.h"
#include "LyricsDB.h"
#include "../../LyricsLib/LyricsKeywordFilter.h"


/*
 // crc32 用于比较歌词是否已经上传过.

 CREATE TABLE lyrics (
         id integer primary key AUTOINCREMENT, content_type integer, media_length integer,
         rate_total integer DEFAULT 0, rate_count integer DEFAULT 0, dl_count integer DEFAULT 0,
         uploader_id integer, upload_time integer, artist text, artistcmp text, album text,
         title text, titlecmp text, related_link text, edited_by text, digest integer
     );

 CREATE INDEX lyrics_artistcmp on lyrics (artistcmp);
 CREATE INDEX lyrics_titlecmp on lyrics (titlecmp);
 CREATE INDEX lyrics_uploader_id on lyrics (uploader_id);

 ALTER TABLE lyrics ADD COLUMN digest integer;
*/

#define SELECT_LYR_FIELDS   "SELECT artist, title, album, edited_by, related_link, media_length, rate_count, dl_count, rate_total, uploader_id, id, content_type"

#define SQL_SEARCH_LYR_BY_ARTIST_TITLE_ID SELECT_LYR_FIELDS  " FROM lyrics WHERE id = ? AND artistcmp = ? AND titlecmp = ?"
#define SQL_SEARCH_LYR_BY_ARTIST SELECT_LYR_FIELDS           " FROM lyrics WHERE artistcmp = ? LIMIT 50"
#define SQL_SEARCH_LYR_BY_TITLE SELECT_LYR_FIELDS            " FROM lyrics WHERE titlecmp = ? LIMIT 50"
#define SQL_SEARCH_LYR_BY_ARTIST_TITLE SELECT_LYR_FIELDS     " FROM lyrics WHERE artistcmp = ? and titlecmp = ? LIMIT 50"
#define SQL_SEARCH_LYR_BY_ARTIST_TITLE_CONTENT_TYPE SELECT_LYR_FIELDS " FROM lyrics WHERE artistcmp = ? and titlecmp = ? and content_type = ? LIMIT 50"

#define SQL_GET_LYR_DIGEST_BY_ID "SELECT digest FROM lyrics WHERE id = ?"
#define SQL_UPDATE_LYR_DIGEST_BY_ID "UPDATE lyrics SET digest = ? WHERE id = ?"

#define SQL_ADD_LYRICS  "INSERT INTO lyrics (artist, title, album, edited_by, related_link, media_length, rate_count, dl_count, rate_total, uploader_id, content_type, upload_time, artistcmp, titlecmp, digest) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"

#define SQL_UPDATE_LYRICS_LINK_BY_ID "UPDATE lyrics SET related_link = ? WHERE id = ?"
#define SQL_UPDATE_LYRICS_PROPS_BY_ID "UPDATE lyrics SET artist = ?, title = ?, album = ?, media_length = ?, upload_time = ?, digest = ?  WHERE id = ?"


void SqlGetLyrics(sqlite3_stmt *sqlStmt, RetLyrInfo &lyricsInfo) {
    int n = 0;
    const char *text;
    int rateTotal;

    // lyricsInfo. = sqlite3_column_int(sqlStmt, n++);
    SQLITE3_GET_TEXT_FIELD(sqlStmt, lyricsInfo.strArtist);
    SQLITE3_GET_TEXT_FIELD(sqlStmt, lyricsInfo.strTitle);
    SQLITE3_GET_TEXT_FIELD(sqlStmt, lyricsInfo.strAlbum);
    SQLITE3_GET_TEXT_FIELD(sqlStmt, lyricsInfo.strUploader);
    SQLITE3_GET_TEXT_FIELD(sqlStmt, lyricsInfo.strLink);

    SQLITE3_GET_INT_FIELD(sqlStmt, lyricsInfo.nTimeLength);
    SQLITE3_GET_INT_FIELD(sqlStmt, lyricsInfo.nRateCount);
    SQLITE3_GET_INT_FIELD(sqlStmt, lyricsInfo.nDownloads);
    SQLITE3_GET_INT_FIELD(sqlStmt, rateTotal);
    SQLITE3_GET_INT_FIELD(sqlStmt, lyricsInfo.uploaderID);
    SQLITE3_GET_INT_FIELD(sqlStmt, lyricsInfo.lyricsID);
    SQLITE3_GET_INT_FIELD(sqlStmt, lyricsInfo.contentType);

    if (lyricsInfo.nRateCount) {
        lyricsInfo.fRate = rateTotal / lyricsInfo.nRateCount;
    }
}


//////////////////////////////////////////////////////////////////////

int LyricsDB::init(const string &dataPath) {
    string fileName = dirStringJoin(dataPath, "database/lyrics.db");
    LOG(INFO) << "Open lyrics data base: " << fileName;
    int ret = sqlite3_open(fileName.c_str(), &m_db);
    if (ret != SQLITE_OK) {
        printf("Open user db FAILED: %s", fileName.c_str());
        return ERR_FALSE;
    }

    string fn = dirStringJoin(dataPath, "data/dxTitleAlias.xml");
    if (LoadTitleAliasDb(fn.c_str()) != ERR_OK) {
        printf("LoadTitleAlias file: %s FAILED!", fn.c_str());
    }

    // Alias artist
    fn = dirStringJoin(dataPath, "data/dxArtistAlias.xml");
    if (LoadArtistAliasDb(fn.c_str()) != ERR_OK) {
        printf("LoadArtistAlias file: %s FAILED!", fn.c_str());
    }

    SQLIT3_STMT_PREPARE(m_db, SQL_ADD_LYRICS, m_stmtAddLyrics);
    SQLIT3_STMT_PREPARE(m_db, SQL_UPDATE_LYRICS_LINK_BY_ID, m_stmtUpdateLyrLinkById);
    SQLIT3_STMT_PREPARE(m_db, SQL_UPDATE_LYRICS_PROPS_BY_ID, m_stmtUpdateLyrPropsById);

    SQLIT3_STMT_PREPARE(m_db, SQL_GET_LYR_DIGEST_BY_ID, m_stmtGetLyricsDigestById);
    SQLIT3_STMT_PREPARE(m_db, SQL_UPDATE_LYR_DIGEST_BY_ID, m_stmtUpdateLyricsDigestById);

    SQLIT3_STMT_PREPARE(m_db, SQL_SEARCH_LYR_BY_ARTIST_TITLE_ID, m_stmtSearchLyrByArtistTitleId);
    SQLIT3_STMT_PREPARE(m_db, SQL_SEARCH_LYR_BY_ARTIST, m_stmtSearchLyrByArtist);
    SQLIT3_STMT_PREPARE(m_db, SQL_SEARCH_LYR_BY_TITLE, m_stmtSearchLyrByTitle);
    SQLIT3_STMT_PREPARE(m_db, SQL_SEARCH_LYR_BY_ARTIST_TITLE, m_stmtSearchLyrByArtistTitle);
    SQLIT3_STMT_PREPARE(m_db, SQL_SEARCH_LYR_BY_ARTIST_TITLE_CONTENT_TYPE, m_stmtSearchLyrByArtistTitleContentType);

    return ERR_OK;
}

void LyricsDB::Quit() {
}

int LyricsDB::AddLyrics(LyricsInfo &lyrInfo) {
    int ret = ERR_OK, n = 1;

    lyrInfo.uploadDate = time(nullptr);

    sqlite3_reset(m_stmtAddLyrics);

    SQLITE3_BIND_TEXT(m_db, m_stmtAddLyrics, lyrInfo.artist.c_str());
    SQLITE3_BIND_TEXT(m_db, m_stmtAddLyrics, lyrInfo.title.c_str());
    SQLITE3_BIND_TEXT(m_db, m_stmtAddLyrics, lyrInfo.album.c_str());
    SQLITE3_BIND_TEXT(m_db, m_stmtAddLyrics, lyrInfo.by.c_str());
    SQLITE3_BIND_TEXT(m_db, m_stmtAddLyrics, lyrInfo.relatedHttpLink.c_str());

    SQLITE3_BIND_INT(m_db, m_stmtAddLyrics, lyrInfo.getMediaLengthInt());
    SQLITE3_BIND_INT(m_db, m_stmtAddLyrics, (int)lyrInfo.rateCount);
    SQLITE3_BIND_INT(m_db, m_stmtAddLyrics, (int)lyrInfo.downloadCount);
    SQLITE3_BIND_INT(m_db, m_stmtAddLyrics, (int)lyrInfo.rateTotal);
    SQLITE3_BIND_INT64(m_db, m_stmtAddLyrics, lyrInfo.uploaderId);
    SQLITE3_BIND_INT(m_db, m_stmtAddLyrics, lyrInfo.lyrContentType);
    SQLITE3_BIND_INT64(m_db, m_stmtAddLyrics, lyrInfo.uploadDate);

    SQLITE3_BIND_TEXT(m_db, m_stmtAddLyrics, lyrInfo.arCmp.c_str());
    SQLITE3_BIND_TEXT(m_db, m_stmtAddLyrics, lyrInfo.tiCmp.c_str());

    SQLITE3_BIND_INT(m_db, m_stmtAddLyrics, (int)lyrInfo.digest);

    ret = sqlite3_step(m_stmtAddLyrics);
    if (ret == SQLITE_ERROR) {
        LOG(ERROR) << "Failed to add lyrics, error: " << sqlite3_errmsg(m_db);
        ret = ERR_DATABASE_ERROR;
    } else {
        lyrInfo.lyricsID = sqlite3_last_insert_rowid(m_db);
        ret = ERR_OK;
        DLOG(INFO) << "Add lyrics successfully, artist: " << lyrInfo.artist << ", title: " << lyrInfo.title << ", id: " << lyrInfo.lyricsID;
    }

    sqlite3_reset(m_stmtAddLyrics);

    return ret;
}

int LyricsDB::UpdateLyricsPropById(long nId, LyricsInfo &lyrInfo) {
    int ret = ERR_OK, n = 1;

    lyrInfo.uploadDate = time(nullptr);

    sqlite3_reset(m_stmtUpdateLyrPropsById);

    SQLITE3_BIND_TEXT(m_db, m_stmtUpdateLyrPropsById, lyrInfo.artist.c_str());
    SQLITE3_BIND_TEXT(m_db, m_stmtUpdateLyrPropsById, lyrInfo.title.c_str());
    SQLITE3_BIND_TEXT(m_db, m_stmtUpdateLyrPropsById, lyrInfo.album.c_str());

    SQLITE3_BIND_INT(m_db, m_stmtUpdateLyrPropsById, lyrInfo.getMediaLengthInt());
    SQLITE3_BIND_INT64(m_db, m_stmtUpdateLyrPropsById, lyrInfo.uploadDate);
    SQLITE3_BIND_INT(m_db, m_stmtUpdateLyrPropsById, lyrInfo.digest);

    SQLITE3_BIND_INT(m_db, m_stmtUpdateLyrPropsById, (int)nId);

    ret = sqlite3_step(m_stmtUpdateLyrPropsById);
    if (ret == SQLITE_ERROR) {
        LOG(ERROR) << "Failed to update lyrics info, error: " << sqlite3_errmsg(m_db);
        ret = ERR_DATABASE_ERROR;
    } else {
        ret = ERR_OK;
        DLOG(INFO) << "Update lyrics info successfully, artist: " << lyrInfo.artist << ", title: " << lyrInfo.title << ", id: " << lyrInfo.lyricsID;
    }

    sqlite3_reset(m_stmtUpdateLyrPropsById);

    return ret;
}

int LyricsDB::UpdateLyricsLinkById(long nId, cstr_t szLink) {
    int ret = ERR_OK, n = 1;

    sqlite3_reset(m_stmtUpdateLyrLinkById);

    SQLITE3_BIND_TEXT(m_db, m_stmtUpdateLyrLinkById, szLink);

    SQLITE3_BIND_INT(m_db, m_stmtUpdateLyrLinkById, (int)nId);

    ret = sqlite3_step(m_stmtUpdateLyrLinkById);
    if (ret == SQLITE_ERROR) {
        LOG(ERROR) << "Failed to update lyrics link, error: " << sqlite3_errmsg(m_db);
        ret = ERR_DATABASE_ERROR;
    } else {
        ret = ERR_OK;
        DLOG(INFO) << "Update lyrics link successfully, link: " << szLink << ", id: " << nId;
    }

    sqlite3_reset(m_stmtUpdateLyrLinkById);

    return ret;
}

int LyricsDB::getLyricsDigest(long id, uint32_t &digestOut) {
    int ret = ERR_OK, n = 1;

    sqlite3_reset(m_stmtGetLyricsDigestById);

    SQLITE3_BIND_INT(m_db, m_stmtGetLyricsDigestById, (int)id);

    ret = sqlite3_step(m_stmtGetLyricsDigestById);
    if (ret == SQLITE_ERROR) {
        LOG(ERROR) << "Failed to get lyrics digest, error: " << sqlite3_errmsg(m_db);
        ret = ERR_DATABASE_ERROR;
    } else {
        digestOut = sqlite3_column_int(m_stmtGetLyricsDigestById, 0);
        DLOG(INFO) << "Get lyrics digest successfully, digest: " << digestOut << ", id: " << id;
        ret = ERR_OK;
    }

    sqlite3_reset(m_stmtGetLyricsDigestById);

    return ret;
}

int LyricsDB::updateLyricsDigest(long id, uint32_t digest) {
    int ret = ERR_OK, n = 1;

    sqlite3_reset(m_stmtUpdateLyricsDigestById);

    SQLITE3_BIND_INT(m_db, m_stmtUpdateLyricsDigestById, (int)digest);
    SQLITE3_BIND_INT(m_db, m_stmtUpdateLyricsDigestById, (int)id);

    ret = sqlite3_step(m_stmtUpdateLyricsDigestById);
    if (ret == SQLITE_ERROR) {
        LOG(ERROR) << "Failed to update lyrics digest, error: " << sqlite3_errmsg(m_db);
        ret = ERR_DATABASE_ERROR;
    } else {
        DLOG(INFO) << "Update lyrics digest successfully, digest: " << digest << ", id: " << id;
        ret = ERR_OK;
    }

    sqlite3_reset(m_stmtUpdateLyricsDigestById);

    return ret;
}

// Search lyrics by id
int LyricsDB::SearchLyricsByArtistTitleID(cstr_t szArtist, cstr_t szTitle, long nID, RetLyrInfo &lyrInfo) {
    int ret = ERR_OK, n = 1;

    sqlite3_reset(m_stmtSearchLyrByArtistTitleId);

    SQLITE3_BIND_INT(m_db, m_stmtSearchLyrByArtistTitleId, (int)nID);
    SQLITE3_BIND_TEXT(m_db, m_stmtSearchLyrByArtistTitleId, szArtist);
    SQLITE3_BIND_TEXT(m_db, m_stmtSearchLyrByArtistTitleId, szTitle);

    ret = sqlite3_step(m_stmtSearchLyrByArtistTitleId);
    if (ret == SQLITE_ERROR) {
        LOG(ERROR) << "Failed to search lyrics, error: " << sqlite3_errmsg(m_db);
        ret = ERR_DATABASE_ERROR;
    } else if (ret == SQLITE_ROW) {
        SqlGetLyrics(m_stmtSearchLyrByArtistTitleId, lyrInfo);
        ret = ERR_OK;
    } else {
        ret = ERR_NOT_FOUND;
    }

    sqlite3_reset(m_stmtSearchLyrByArtistTitleId);

    return ret;
}

// Search lyrics by artist and title
int LyricsDB::SearchLyricsByArtistTitle(cstr_t szArtist, cstr_t szTitle, RetLyrInfoList &vLyrics) {
    int ret = ERR_OK, n = 1;

    sqlite3_reset(m_stmtSearchLyrByArtistTitle);

    SQLITE3_BIND_TEXT(m_db, m_stmtSearchLyrByArtistTitle, szArtist);
    SQLITE3_BIND_TEXT(m_db, m_stmtSearchLyrByArtistTitle, szTitle);

    return getSearchLyricsResults(m_stmtSearchLyrByArtistTitle, vLyrics);
}

// Search lyrics by artist and title
int LyricsDB::SearchLyricsByArtistTitleContentType(cstr_t szArtist, cstr_t szTitle, RetLyrInfoList &vLyrics, uint32_t contentType) {
    int ret = ERR_OK, n = 1;

    sqlite3_reset(m_stmtSearchLyrByArtistTitleContentType);

    SQLITE3_BIND_TEXT(m_db, m_stmtSearchLyrByArtistTitleContentType, szArtist);
    SQLITE3_BIND_TEXT(m_db, m_stmtSearchLyrByArtistTitleContentType, szTitle);
    SQLITE3_BIND_INT(m_db, m_stmtSearchLyrByArtistTitleContentType, contentType);

    return getSearchLyricsResults(m_stmtSearchLyrByArtistTitleContentType, vLyrics);
}

int LyricsDB::SearchLyricsByTitle(cstr_t szTitle, RetLyrInfoList &vLyrics) {
    int ret = ERR_OK, n = 1;

    sqlite3_reset(m_stmtSearchLyrByTitle);

    SQLITE3_BIND_TEXT(m_db, m_stmtSearchLyrByTitle, szTitle);

    return getSearchLyricsResults(m_stmtSearchLyrByTitle, vLyrics);
}

int LyricsDB::SearchLyricsByArtist(cstr_t szArtist, RetLyrInfoList &vLyrics) {
    int ret = ERR_OK, n = 1;

    sqlite3_reset(m_stmtSearchLyrByArtist);

    SQLITE3_BIND_TEXT(m_db, m_stmtSearchLyrByArtist, szArtist);

    return getSearchLyricsResults(m_stmtSearchLyrByArtist, vLyrics);
}

cstr_t LyricsDB::GetAliasTitleStr(cstr_t szStrOrg) {
    MAP_STR::iterator it;
    it = m_mapTiAlias.find(szStrOrg);
    if (it != m_mapTiAlias.end()) {
        return (*it).second.c_str();
    } else {
        return NULL;
    }
}

cstr_t LyricsDB::GetAliasArtistStr(cstr_t szStrOrg) {
    MAP_STR::iterator it;
    it = m_mapArAlias.find(szStrOrg);
    if (it != m_mapArAlias.end()) {
        return (*it).second.c_str();
    } else {
        return NULL;
    }
}

void LyricsDB::SetAliasTitle(cstr_t szTitleOrg, cstr_t szTitleAlias) {
    m_mapTiAlias[szTitleOrg] = szTitleAlias;
}

void LyricsDB::SetAliasArtist(cstr_t szArtistOrg, cstr_t szArtistAlias) {
    m_mapArAlias[szArtistOrg] = szArtistAlias;
}

int LyricsDB::LoadTitleAliasDb(cstr_t szFile) {
    CSimpleXML xml;
    string strOrg, strAlias;

    if (!xml.parseFile(szFile)) {
        LOG(ERROR) << "Parse xml file FAILED: " << szFile;
        return ERR_PARSE_XML;
    }
    if (strcmp(xml.m_pRoot->name.c_str(), "stralias") != 0) {
        return ERR_FALSE;
    }

    for (SXNode::iterator it = xml.m_pRoot->listChildren.begin(); it != xml.m_pRoot->listChildren.end(); it++) {
        SXNode *pNode;
        pNode = *it;
        if (strcmp(pNode->name.c_str(), "item") == 0) {
            const char *szText;
            szText = pNode->getProperty("org");
            if (szText) {
                CLyricsKeywordFilter::filter(szText, strOrg);
                CLyricsKeywordFilter::filter(pNode->getPropertySafe("alias"), strAlias);
                SetAliasTitle(strOrg.c_str(), strAlias.c_str());
            }
        }
    }

    return ERR_OK;
}

int LyricsDB::LoadArtistAliasDb(cstr_t szFile) {
    CSimpleXML xml;
    string strOrg, strAlias;

    if (!xml.parseFile(szFile)) {
        LOG(ERROR) << "Parse xml file FAILED: " << szFile;
        return ERR_PARSE_XML;
    }
    if (strcmp(xml.m_pRoot->name.c_str(), "stralias") != 0) {
        return ERR_FALSE;
    }

    for (SXNode::iterator it = xml.m_pRoot->listChildren.begin(); it != xml.m_pRoot->listChildren.end(); it++) {
        SXNode *pNode;
        pNode = *it;
        if (strcmp(pNode->name.c_str(), "item") == 0) {
            const char *szText;
            szText = pNode->getProperty("org");
            if (szText) {
                CLyricsKeywordFilter::filter(szText, strOrg);
                CLyricsKeywordFilter::filter(pNode->getPropertySafe("alias"), strAlias);
                SetAliasArtist(strOrg.c_str(), strAlias.c_str());
            }
        }
    }

    return ERR_OK;
}

int LyricsDB::getSearchLyricsResults(sqlite3_stmt *stmt, RetLyrInfoList &vLyrics) {
    int ret;

    while (true) {
        ret = sqlite3_step(stmt);
        if (ret == SQLITE_ERROR) {
            LOG(ERROR) << "Failed to get lyrics result, error: " << sqlite3_errmsg(m_db);
            ret = ERR_DATABASE_ERROR;
            break;
        } else if (ret == SQLITE_ROW) {
            RetLyrInfo lyrInfo;
            SqlGetLyrics(stmt, lyrInfo);
            vLyrics.push_back(lyrInfo);
        } else {
            ret = ERR_OK;
            break;
        }
    }

    sqlite3_reset(stmt);

    return ret;
}
