#!/bin/bash

CUR_DIR="${BASH_SOURCE-$0}"
CUR_DIR=`dirname ${CUR_DIR}`
cd ${CUR_DIR}

function exit_if_err() {
    rc=$?
    if [ $rc -ne 0 ]; then
        echo $*
        exit $rc
    fi
}

sqlite3 lyrics.db < create-lyrics-db.sql
exit_if_err "Failed to create lyrics database."

echo "Converting lyrics database..."
sqlite3 lyrics.db < port-lyrics-db.sql
exit_if_err "Failed to convert lyrics database."
echo "Done!"


sqlite3 users.db < create-users-db.sql
exit_if_err "Failed to create users database."

echo "Converting users database..."
sqlite3 users.db < port-users-db.sql
exit_if_err "Failed to convert users database."
echo "Done!"
