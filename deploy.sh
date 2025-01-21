#!/bin/bash

CUR_DIR=`dirname ${BASH_SOURCE-$0}`
cd ${CUR_DIR}
CUR_DIR="$(pwd)"

# https://openresty.org/en/linux-packages.html
echo "TO install openresty, follow instructions here: https://openresty.org/en/linux-packages.html"

function exit_if_err() {
    rc=$?
    if [ $rc -ne 0 ]; then
        echo $*
        exit $rc
    fi
}

target=release
is_master=1

while (($# > 0)); do
    case "$1" in
        "debug")
            target="debug"
        ;;

        "release")
            target="release"
        ;;

        "slave")
            is_master=0
        ;;

        *)
            echo "Invalid parameters. ($1)"
            exit 1
        ;;
    esac
    shift
done

build_dir="build/$target"
prod_src="${CUR_DIR}/product"
dst_dir="/mlserver"

mkdir -p $dst_dir

systemctl stop lyrics-server

rsync -rptv --delete --exclude lyrics-server.ini ${prod_src}/bin /mlserver/
rsync -rptv --delete ${prod_src}/conf /mlserver/
rsync -rptv --delete ${prod_src}/logrotate /mlserver/
rsync -rptv --delete --exclude conf.lua ${prod_src}/lua /mlserver/
rsync -rptv --delete ${prod_src}/www-templates /mlserver/
rsync -rptv --delete ${prod_src}/www-static /mlserver/

cp "${build_dir}/lyrics-server" /mlserver/bin/
cp ${prod_src}/.gitignore /mlserver/

cd /mlserver

if ! test -d .git ; then
    echo "Init git..."
    git init .
    echo "OK"
fi

# 创建数据库文件
mkdir -p database logs logs/lyrics-server lyrics/lu8 temp data-sync-log

if ! test -f database/lyrics.db ; then
    echo "Create databases..."
    cp ${prod_src}/database/upgrade-v1/create*.sql /mlserver/database/

    sqlite3 database/lyrics.db < database/create-lyrics-db.sql
    exit_if_err "Failed to create lyrics database."

    sqlite3 database/users.db < database/create-users-db.sql
    exit_if_err "Failed to create users database."
    echo "OK"
fi

# lyrics-server
# master port: 8101
if ! test -f bin/lyrics-server.ini ; then
    # lyrics-server.ini
    echo "Creating lyrics-server.ini ..."

    SYNC_KEYS=$(python3 -c "import random, string;print(''.join(random.choice(string.ascii_uppercase + string.digits) for _ in range(32)))")
    exit_if_err "Failed to generate SYNC-KEY."

    echo "[main]
root-dir=/mlserver
upload-dir-name=lu8
http-lyrics-url-base=http://search.crintsoft.com/l/
port=8101
sync-duration=60
sync-master-url=http://search.crintsoft.com/api-i/data-sync
aes-key=${SYNC_KEYS}
master=${is_master}" > bin/lyrics-server.ini
    exit_if_err "Failed to create lyrics-server.ini."
    echo "OK"
fi

# 替换 lua/conf.lua
if ! test -f "lua/conf.lua" ; then
    echo "Creating lua/conf.lua ..."

    KEYS=$(python3 -c "import random, string;print(''.join(random.choice(string.ascii_uppercase + string.digits) for _ in range(16)))")
    echo "local _M = {}
_M.key = '${KEYS}'
_M.server = '127.0.0.1'
_M.port = 8101
_M.ut_enabled = false

return _M" >> lua/conf.lua
    exit_if_err "Failed to config lua database-api config."
    echo "OK"
fi

# 配置 log rotate
rsync -rptv /mlserver/logrotate/nginx.conf /etc/logrotate.d/nginx.conf
exit_if_err "Failed to config nginx log rotate config."

# Install lyrics-server service
rsync -rptv /mlserver/bin/lyrics-server.service /etc/systemd/system/lyrics-server.service
systemctl enable lyrics-server
systemctl start lyrics-server
exit_if_err "Failed to install lyrics-server service."
# service lyrics-server status

# 配置 openresty
opm get bungle/lua-resty-template
opm get GUI/lua-resty-mail
opm get ledgetech/lua-resty-http

cd -

echo "配置成功，需要手动在 /etc/openresty/nginx.conf 中包含 mlserver.conf"
echo "sudo vi /etc/openresty/nginx.conf"
echo "include /mlserver/conf/mlserver.conf;"

openresty -s reload
