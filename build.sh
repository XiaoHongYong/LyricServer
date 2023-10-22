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

target=release
is_clean=false
ut=OFF

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

build_dir="build/$target"

mkdir -p $build_dir
cd $build_dir
cmake ../../LyricsServer -DCMAKE_BUILD_TYPE=$target -DUT=$ut
exit_if_err "Failed to generate cmake file of LyricsServer."

make -j `grep -c processor /proc/cpuinfo`
exit_if_err "Failed to make LyricsServer."
