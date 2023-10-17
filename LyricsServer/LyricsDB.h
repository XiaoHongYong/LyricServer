#pragma once

#include "Sqlite3Utils.h"
#include "LyricsInfo.hpp"
#include "MLProtocol/MLProtocol.h"


class LyricsDB {
public:
    ~LyricsDB();

    int init(const string &dataPath);
    void Quit();

    int AddLyrics(LyricsInfo &lyrInfo);
    int UpdateLyricsPropById(long nId, LyricsInfo &lyrProperties);
    int UpdateLyricsLinkById(long nId, cstr_t szLink);

    int getLyricsDigest(long id, uint32_t &digestOut);
    int updateLyricsDigest(long id, uint32_t digest);

    int isLyricsExist(cstr_t szArtist, cstr_t szTitle, uint32_t digest);
    int SearchLyricsByArtistTitleID(cstr_t szArtist, cstr_t szTitle, long nID, RetLyrInfo &lyrInfo);
    int SearchLyricsByArtist(cstr_t szArtist, RetLyrInfoList &vLyrics);
    int SearchLyricsByTitle(cstr_t szTitle, RetLyrInfoList &vLyrics);
    int SearchLyricsByArtistTitle(cstr_t szArtist, cstr_t szTitle, RetLyrInfoList &vLyrics);
    int SearchLyricsByArtistTitleContentType(cstr_t szArtist, cstr_t szTitle, RetLyrInfoList &vLyrics, uint32_t contentType);

    sqlite3 *db() { return m_db; }

public:
    //
    // string Alias
    //
    int LoadArtistAliasDb(cstr_t szFile);
    int LoadTitleAliasDb(cstr_t szFile);

    cstr_t GetAliasTitleStr(cstr_t szStrOrg);
    cstr_t GetAliasArtistStr(cstr_t szStrOrg);

    void SetAliasTitle(cstr_t szTitleOrg, cstr_t szTitleAlias);
    void SetAliasArtist(cstr_t szArtistOrg, cstr_t szArtistAlias);

protected:
    int getSearchLyricsResults(sqlite3_stmt *stmt, RetLyrInfoList &vLyrics);

protected:
    typedef map<string, string>     MAP_STR;
    typedef std::set<string>        SET_STR;

    //
    // Alias artist or artist
    //
    MAP_STR                     m_mapArAlias, m_mapTiAlias;

    sqlite3                     *m_db = nullptr;

    sqlite3_stmt                *m_stmtAddLyrics = nullptr;
    sqlite3_stmt                *m_stmtUpdateLyrLinkById = nullptr;
    sqlite3_stmt                *m_stmtUpdateLyrPropsById = nullptr;
    sqlite3_stmt                *m_stmtGetLyricsDigestById = nullptr;
    sqlite3_stmt                *m_stmtUpdateLyricsDigestById = nullptr;

    sqlite3_stmt                *m_stmtSearchLyrByArtistTitleId = nullptr;
    sqlite3_stmt                *m_stmtSearchLyrByArtist = nullptr;
    sqlite3_stmt                *m_stmtSearchLyrByTitle = nullptr;
    sqlite3_stmt                *m_stmtSearchLyrByArtistTitle = nullptr;
    sqlite3_stmt                *m_stmtSearchLyrByArtistTitleContentType = nullptr;
    sqlite3_stmt                *m_stmtSearchLyrByArtistTitleDigest = nullptr;

};
