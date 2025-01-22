#!/bin/bash

CUR_DIR=`dirname ${BASH_SOURCE-$0}`
cd ${CUR_DIR}
CUR_DIR="$(pwd)"


function exit_if_err() {
    rc=$?
    if [ $rc -ne 0 ]; then
        echo $*
        exit $rc
    fi
}

function clean() {
    echo "Clean build..."
    rm -rf build
}

function create_lyrics_server_ini() {
    # lyrics-server
    # master port: 8101
    if ! test -f $build_dir/lyrics-server.ini ; then
        # lyrics-server.ini
        echo "Creating $build_dir/lyrics-server.ini ..."

        SYNC_KEYS=$(python3 -c "import random, string;print(''.join(random.choice(string.ascii_uppercase + string.digits) for _ in range(32)))")
        exit_if_err "Failed to generate SYNC-KEY."

        echo "[main]
root-dir=${CUR_DIR}/product
upload-dir-name=lu8
http-lyrics-url-base=http://search.crintsoft.com/l/
port=8101
sync-duration=60
sync-master-url=http://search.crintsoft.com/api-i/data-sync
aes-key=${SYNC_KEYS}
master=${is_master}" > $build_dir/lyrics-server.ini
        exit_if_err "Failed to create lyrics-server.ini."
        echo "OK"
    fi
}

target=release
is_clean=false
ut=OFF
is_master=1

while (($# > 0)); do
    case "$1" in
        "debug")
            target="debug"
        ;;

        "release")
            target="release"
        ;;

        "clean")
            is_clean=true
        ;;

        "ut")
            ut=ON
        ;;

        *)
            echo "Invalid parameters. ($1)"
            exit 1
        ;;
    esac
    shift
done

if [ "$is_clean" == "true" ]; then
    clean
fi

build_dir="${CUR_DIR}/build/$target"

mkdir -p $build_dir
cd $build_dir
cmake ../../LyricsServer -DCMAKE_BUILD_TYPE=$target -DUT=$ut
exit_if_err "Failed to generate cmake file of LyricsServer."

make -j `grep -c processor /proc/cpuinfo`
exit_if_err "Failed to make LyricsServer."

create_lyrics_server_ini