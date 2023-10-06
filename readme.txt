== 目录说明

* LyricsServer:
  * 提供给客户端的歌词搜索、上传、登录服务
  * 数据库 API
  * 监听在 127.0.0.1 内部端口，未做鉴权服务
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


