CREATE TABLE lyrics (
        id integer primary key AUTOINCREMENT, content_type integer, media_length integer,
        rate_total integer DEFAULT 0, rate_count integer DEFAULT 0, dl_count integer DEFAULT 0,
        uploader_id integer, upload_time integer, artist text, artistcmp text, album text,
        title text, titlecmp text, related_link text, edited_by text, digest integer
    );

CREATE INDEX lyrics_artistcmp on lyrics (artistcmp);
CREATE INDEX lyrics_titlecmp on lyrics (titlecmp);
CREATE INDEX lyrics_uploader_id on lyrics (uploader_id);
