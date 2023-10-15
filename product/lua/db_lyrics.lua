local db_api = require "database_api"

-- local SELECT_LYRICS_FIELDS = "SELECT artist, title, album, related_link, edited_by, dl_count, media_length, rate_count, rate_total "

-- local stmtSearchLyricsByTitle = assert( db:prepare(SELECT_LYRICS_FIELDS .. "FROM lyrics WHERE titlecmp=? LIMIT 50") )
-- local stmtSearchLyricsByArtist = assert( db:prepare(SELECT_LYRICS_FIELDS .. "FROM lyrics WHERE artistcmp=? LIMIT 50") )
-- local stmtSearchLyricsByArtistAndTitle = assert( db:prepare(SELECT_LYRICS_FIELDS .. "FROM lyrics WHERE titlecmp=? and artistcmp=? LIMIT 50") )

local _M = {}

-- function _M.searchByArtistAndTitle(artistcmp, titlecmp)
--     stmtSearchLyricsByArtistAndTitle:bind_values(artistcmp, titlecmp)
--     return stmtSearchLyricsByArtistAndTitle:nrows()
-- end

-- function _M.searchByTitle(titlecmp)
--     stmtSearchLyricsByTitle:bind_values(titlecmp)
--     return stmtSearchLyricsByTitle:nrows()
-- end

-- function _M.searchByArtist(artistcmp)
--     stmtSearchLyricsByArtist:bind_values(artistcmp)
--     return stmtSearchLyricsByArtist:nrows()
-- end

local sql_select_lyrics_by_user_id = "SELECT related_link, artist, title, album, dl_count, media_length, rate_count, rate_total, upload_time, id FROM lyrics WHERE uploader_id=? ORDER BY upload_time DESC LIMIT 50 OFFSET ?"

function _M.getLyricsByUserId(user_id, offset)
    return db_api.lyrics_array(sql_select_lyrics_by_user_id, {user_id, offset})
end

function _M.getLyricsByUserIdColumns()
    return db_api.lyrics_column_names(sql_select_lyrics_by_user_id)
end

function _M.getLyricsById(id)
    return db_api.lyrics_r1("SELECT * FROM lyrics WHERE id = ?", {id})
end

function _M.deleteLyricsByIdAndUserId(id, user_id)
    local info = _M.getLyricsById(id)
    if info == nil or info.uploader_id ~= user_id then
        return false;
    end

    if db_api.lyrics_delete_by_id(id) == nil then
        return false
    end

    db_api.lyrics_file_delete(info.related_link)

    return true
end

function _M.getCountLyricsByUserId(user_id)
    return db_api.lyrics_r1_1("SELECT COUNT(*) FROM lyrics WHERE uploader_id=?", {user_id})
end

return _M