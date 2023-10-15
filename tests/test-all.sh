#!/bin/bash

CUR_DIR=`dirname ${BASH_SOURCE-$0}`
cd ${CUR_DIR}
CUR_DIR="$(pwd)"

prod_src="${CUR_DIR}/../product"
dir_server_master="${CUR_DIR}/test_env/master"
dir_server_slave="${CUR_DIR}/test_env/slave"

target="debug"
is_clean=false

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

        *)
            echo "Invalid parameters. ($1)"
            exit 1
        ;;
    esac
    shift
done

function exit_if_err() {
    rc=$?
    if [ $rc -ne 0 ]; then
        echo $*
        exit $rc
    fi
}

function copy_server() {
    # 将运行需要的文件复制过去
    dst_dir=$1
    is_master=$2
    mkdir -p $dst_dir
    cp -r ${prod_src}/data ${dst_dir}/
    cp -r ${prod_src}/lua ${dst_dir}/
    cp -r ${prod_src}/conf-test ${dst_dir}/
    cp ${prod_src}/database/upgrade-v1/create*.sql ${dst_dir}/
    cp "${CUR_DIR}/../build/$target/LyricsServer" ${dst_dir}/
    cp "${CUR_DIR}/../build/$target/client" ${dst_dir}/

    cd ${dst_dir}

    # 创建数据库文件
    mkdir database logs lyrics lyrics/lu8 temp data-sync-log
    sqlite3 database/lyrics.db < create-lyrics-db.sql
    sqlite3 database/users.db < create-users-db.sql

    # lyrics-server.ini
    echo "[main]
root-dir=${dst_dir}
upload-dir-name=lu8
http-lyrics-url-base=http://localhost/l/
port=810${is_master}
sync-duration=5
sync-master-url=http://search.crintsoft.com:8201/api-i/data-sync
master=${is_master}" >> lyrics-server.ini

    # LyricsServer
    # master port: 8101
    # slave  port: 8100

    # 替换 conf-test/mlserver.conf
    sed -i '' -e "s/server localhost:8000;/server localhost:810${is_master};/g" ${dst_dir}/conf-test/mlserver.conf
    exit_if_err "Failed to change nginx upstream config."
    sed -i '' -e "s/listen 80;/listen 820${is_master};/g" ${dst_dir}/conf-test/mlserver.conf
    exit_if_err "Failed to change nginx listen config."

    # 替换 lua/conf.lua
    cp -f ${dst_dir}/lua/conf-template.lua ${dst_dir}/lua/conf.lua
    sed -i '' -e "s/http:\/\/127.0.0.1:8000/http:\/\/127.0.0.1:810${is_master}/g" ${dst_dir}/lua/conf.lua
    exit_if_err "Failed to change lua database-api config."

    # nginx
    # master port: 8201
    # slave  port: 8200

    cd -
}

function test_setup() {
    # 创建测试环境
    # * 一个 master server，一个 slave server
    copy_server $dir_server_master 1
    copy_server $dir_server_slave 0
}

function start_services() {
    dst_dir=$1
    is_master=$2

    cd $dst_dir

    $dst_dir/LyricsServer &
    exit_if_err "Failed to start LyricsServer master service."
    sleep 1
    nc -z -w2 127.0.0.1 "810${is_master}"
    exit_if_err "LyricsServer service port is NOT open."

    # 必须切换到 $dst_dir，lua 代码才能正常工作.

    openresty -p $dst_dir -c conf-test/nginx.conf
    exit_if_err "Failed to start openresty master service."
    # $dir_server_slave/LyricsServer &
    cd -
}

function start_all_services() {
    pkill -9 -f LyricsServer
    openresty -s stop

    start_services $dir_server_master 1
    start_services $dir_server_slave 0
}

function stop_services() {
    pkill -9 -f LyricsServer
    openresty -s stop
    # kill -9 $(lsof -ti:8201,8200)
}

function run_lua_ut() {
    result="$(curl http://viewlyrics.com:8201/lua_ut)"
    exit_if_err "Failed to curl lua unittest result."
    if [[ $result == *'"failed":true'* ]]; then
        echo $result | python -m json.tool
        echo 'Lua unittest failed.'
        exit 1
    fi
}

function create_client_user_account() {
    # 直接访问内部数据库 api 创建用户账户: _user_xyz_, _password_
    curl 'http://127.0.0.1:8101/db-modify-api/users' \
        --data-raw '{"fields":"MLPasswordHash,PasswordHash,UserName,Email,CreateDate,LastLoginDate,LastPasswordChangedDate","action":"create","args":["DF5A1B2A3AECB79E","KOM/8cXedktFRRO5zvxmzA==","_user_xyz_","_email@test.com_","2023-10-13 07:04:48","2023-10-13 07:04:48","2023-10-13 07:04:48"]}'
}

function test_lyrics_client() {
    $dir_server_master/client 8201 test-upload-lyrics.lrc
    exit_if_err "Failed test lyrics client."
}

if test -d test_env; then
    echo "Remove latest test environment..."
    stop_services
    rm -rf test_env
    echo "OK"
fi

echo "== Setup test environment..."
test_setup
exit_if_err "Failed to setup test environment."
echo "== OK"

echo "== Start all services..."
start_all_services
echo "== OK"

echo "== Run lua unittest..."
run_lua_ut
echo "== OK"

echo "== Create client user account..."
create_client_user_account
echo "== OK"

echo "== Test lyrics client..."
test_lyrics_client
echo "== OK"

echo "== Stop services..."
stop_services
echo "== OK"

echo "== Done all tests successfully. =="
