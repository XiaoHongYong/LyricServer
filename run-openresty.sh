#!/bin/bash

CUR_DIR=`dirname ${BASH_SOURCE-$0}`
cd ${CUR_DIR}
CUR_DIR="$(pwd)"

cd product
openresty -p $CUR_DIR/product -c conf-test/nginx.conf
