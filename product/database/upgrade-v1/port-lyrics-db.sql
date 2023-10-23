ATTACH '../lyrics.db' AS lyrics_old;

INSERT INTO lyrics (id, content_type, media_length, rate_total, rate_count, dl_count, uploader_id,
upload_time, artist, artistcmp, album, title, titlecmp, related_link, edited_by) 
    SELECT id, content_type, media_length, rate_total, rate_count, dl_count, uploader_id,
    upload_time, artist, artistcmp, album, title, titlecmp, related_link, edited_by FROM lyrics_old.lyrics;
