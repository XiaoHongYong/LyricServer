== 目录说明

* LyricsServer:
  * 提供给客户端的歌词搜索、上传、登录服务
  * 监听在 127.0.0.1 内部端口，未做鉴权服务
  * 数据库 API
    * 路径: 
      * /db-api/user
      * /db-api/lyrics
    * 参数在 POST 中，json 格式
      * action:
        * prepare: prepare statement
          * 参数
            * sql: prepare 的 sql 语句
          * 返回
            * stmt-id: 在执行 query 命令时, 用 stmt-id 来查询对应的 sql statement
            * col-names: 所有的字段名
        * query: 获取查询结果
          * 参数
            * args: 执行 statement 的参数，array 类型，如果无参数，则可不提供 args
            * col-names: 返回 col-names，所有的字段名
          * 返回
            * col-names: 所有的字段名，当 col-names 参数为 true 时才有此字段
            * rows: 查询的数据结构，如果有值，则是一个二维数组，如果没有值，则是空的一维数组
        * exec: 执行一次性的 sql 查询
          * 参数
            * sql: prepare 的 sql 语句
            * args: 执行 statement 的参数，array 类型，如果无参数，则可不提供 args
          * 返回
            * col-names: 所有的字段名
            * rows: 查询的数据结构，如果有值，则是一个二维数组，如果没有值，则是空的一维数组
    * 返回值为 json 格式, 包含下面的通用字段
      * result: string，执行结果，一切正常为 "OK"
      * message: string，如果有错误时，一般都有更详细的错误信息.
    * 请求格式参考 tests/chrome-console-db-api.js
* product:
  * 线上部署的配置
* local:
  * 本地测试部署的配置

== Mac 下开发测试
* 在 xcode 中打开 LyricsServer/LyricsServer.xcodeproj，运行 LyricsServer
  * 需要配置 lyrics-server.ini，保存在 LyricsServer 编译后的目录下
  '''
[main]
root-dir=/path-to-repo/Mp3Player/ServerNgx/product
upload-dir-name=lu7
http-lyrics-url-base=http://search.crintsoft.com/l/
port=8000
  '''
  * 可以在 chrome console 中模拟发送 LyricsAPI 请求，请求格式参考 tests/chrome-console-db-api.js

* openresty:
  * install:
  * /opt/homebrew/opt/openresty/bin/openresty -p `pwd`/ -c conf/nginx.conf 

* 其他工具命令
  * 查看端口所运行的进程: sudo lsof -i :8080
    * kill -9 $(lsof -ti:8080,8081)
  * 测试性能(mac):
    * ab -k -c10 -n10000 -t1 -r 'http://127.0.0.1:8080/'
    * 只有支持 Connection: KeepAlive，性能才会高.
  * 测试 http 协议: tests/request_body_client.py

