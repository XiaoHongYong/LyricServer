#!/usr/bin/env bash
#
# 从 c2 主机同步数据目录到本地 LyricServer/product/data-sync-log/
#
set -euo pipefail

# 远端主机与目录（可按需修改）
REMOTE_HOST="${C2_HOST:-c2}"
REMOTE_PATH="${C2_DATA_SYNC_LOG:-/mlserver/data-sync-log}"

# 本地目标目录（相对脚本所在目录）
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LOCAL_DIR="${SCRIPT_DIR}/product/data-sync-log"

mkdir -p "${LOCAL_DIR}"

echo "==> 从 ${REMOTE_HOST}:${REMOTE_PATH} 同步到 ${LOCAL_DIR}"

# --archive  保留权限/时间戳等（可去掉 --owner --group 避免本地无权限报错）
# --compress 传输压缩
# --delete   删除远端已不存在的文件，保持两边一致
# --partial  断点续传
rsync -av --compress --partial --delete \
    -e ssh \
    "${REMOTE_HOST}:${REMOTE_PATH}/" \
    "${LOCAL_DIR}/"

echo "==> 同步完成"