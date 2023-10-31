## 目录说明

* LyricsServer:
  * 提供给客户端的歌词搜索、上传、登录服务
  * 监听在 127.0.0.1 内部端口，未做鉴权服务
    * 展示内部运行状态: http://localhost:8000/status/
  * 内部 API 通用 JSON 格式定义
    * 使用 POST JSON 传递参数
    * 返回值为 json 格式, 包含下面的通用字段
      * result: string，执行结果，一切正常为 "OK"
      * message: string，如果有错误时，一般都有更详细的错误信息.
    * 请求格式参考 tests/chrome-console-db-api.js
  * 数据库查询 API: 仅仅用于数据查询，不要执行添加、修改、删除.
    * 路径: 
      * /db-api/users
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
  * 数据库修改 API
    * 路径: 
      * /db-modify-api/users
      * /db-modify-api/lyrics
    * 参数在 POST 中，json 格式
      * action:
        * create: 插入新纪录
        * update: 修改纪录
        * delete: 删除
      * fields: create/update 会用到的字段 ID
      * args: 执行 sql 需要填入的参数
    * 返回值为 json 格式
      * id: 当 update 的时候会填入新创建纪录的 ID，其他情况下为 -1
  * 数据同步 API
    * 发送和接收的数据都是加密的
    * 路径:
      * /api-i/data-sync
    * 参数在 POST 中，json 格式
      * filename: 当前同步的文件名，如果为空字符串或者找不到文件，则从第一个文件开始同步
      * offset: 文件偏移位置
    * 返回
      * result: 同步结果字符串
        * OK: 当前成功
      * filename: 当前文件名
      * end: bool, 是否结束
      * offset: 当前返回数据的偏移位置
      * data: 返回的数据
        * 如果已经到结束，则可能无此字段
      * size: 返回数据的大小
  * 歌词文件 API
    * 路径:
      * /lyrics-api/
    * 参数在 POST 中，json 格式
      * action:
        * delete: 删除
      * related-link: 待删除歌词文件的位置.
* product:
  * 线上部署的配置
* local:
  * 本地测试部署的配置

## Mac 下开发测试
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
    * brew install openresty/brew/openresty
  * 依赖其他模块
    * opm get bungle/lua-resty-template
    * opm get GUI/lua-resty-mail
    * opm get ledgetech/lua-resty-http
  * cd ServerNgx/product
  * mkdir database logs lyrics temp lu8 data-sync-log
  * 第一次运行 /opt/homebrew/opt/openresty/bin/openresty -p `pwd` -c conf-test/nginx.conf
  * 如果修改了 nginx.conf，则重启 ningx: /opt/homebrew/opt/openresty/bin/openresty -s reload

* 其他工具命令
  * 查看端口所运行的进程: sudo lsof -i :8080
    * kill -9 $(lsof -ti:8080,8081)
  * 测试性能(mac):
    * ab -k -c10 -n10000 -t1 -r 'http://127.0.0.1:8080/'
    * 只有支持 Connection: KeepAlive，性能才会高.
  * 测试 http 协议: tests/request_body_client.py

## Ubuntu 下编译

sudo mount -t vboxsf -o uid=1000,gid=1000 Mp3Player /home/xhy/Mp3Player

```
sudo apt install cmake
./build.sh release
```

## 多语言支持

* 依赖库:
  * pip install polib
  * https://poedit.net/
* 使用 _TML() 标记需要翻译的字符串
* 使用 www-templates/translate.py 来
  * 提取翻译字符串
    * www-templates/viewlyrics_src, www-templates/crintsoft_src 为原始模板
    * lua 中的字符串也会被提取
  * 生成翻译结果
    * 根据 languages.json 中定义的语言来生成目标语言
      * www-templates/viewlyrics, www-templates/crintsoft 均为自动生成的
    * en.json, zh-cn.json 等为 lua 需要的翻译结果（自动生成，不要手动修改）
    * en.txt, zh-cn.txt 需要翻译的字符串，搜索 todo 查看待翻译的字符串
    * 需检查生成的翻译结果，注意防范因为翻译引入的攻击字符串

## 自动化测试
* 脚本: tests/test-all.sh
  * ./build.sh debug
    * 编译 LyricsServer 和 LyricsClient
  * ./tests/test-all.sh debug clean
    * 测试 debug 版本，并清除测试环境
* Unittest
  * 编译: ./build.sh ut


## 参考开发文档
* Openresty template 语法
  * https://github.com/bungle/lua-resty-template
* Openresty 参考手册
  * https://openresty-reference.readthedocs.io/en/latest/Lua_Nginx_API/#ngxhttp_time
* OpenResty 最佳实践 
  * https://moonbingbing.gitbooks.io/openresty-best-practices/content/
* Ajax axios
  * https://github.com/axios/axios

## Ubuntu 下编译：
    sudo mount -t vboxsf -o uid=1000,gid=1000 zikiplayer /home/henry/zikiplayer
    Fixing your virtualbox shared folder symlink error: https://ahtik.com/fixing-your-virtualbox-shared-folder-symlink-error/
        VBoxManage setextradata YOURVMNAME VBoxInternal2/SharedFoldersEnableSymlinksCreate/YOURSHAREFOLDERNAME 1

    sudo apt-get install systemtap-sdt-dev
    ubuntu: 
    * Configure before build: third-parties/openresty/config-for-ubuntu.sh
    * build.sh
