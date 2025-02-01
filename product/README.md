
## 部署

ssh ubuntu@crintsoft.com

所有数据放在 /mlserver 目录下

### 创建虚拟磁盘
在 Ubuntu 下部署，执行下面的命令.

```
sudo su
mkdir /mlserver
cd /mlserver
# 创建虚拟磁盘：由于普通磁盘的 inodes 数量不够，需要调整，创建虚拟磁盘
dd if=/dev/zero of=/mlserver/lyrics.bin bs=1G count=40
# blocksize 1024, i node bytes 2048
mkfs.ext4 -b 1024 -i 2048 /mlserver/lyrics.bin
mkdir /mlserver/lyrics
mount /mlserver/lyrics.bin /mlserver/lyrics
echo "/mlserver/lyrics.bin /mlserver/lyrics ext4 defaults 0 1" >> /etc/fstab

# df -i to view inodes.

```

### 第一次部署

* 编译环境 
* sudo apt install cmake -y
* sudo apt install build-essential -y

* 代码放在 /home/ubuntu/Mp3Player 目录下
  * 取最新代码，使用 build.sh release 编译
    * cd /home/ubuntu
    * git clone git@gitee.com:xiaohongyong/Mp3Player.git
    * cd /home/ubuntu/Mp3Player
    * git clone git@gitee.com:xiaohongyong/LyrServer.git ServerNgx
    * git clone git@gitee.com:xiaohongyong/mp3player-third-parties.git third-parties
    * git clone git@gitee.com:xiaohongyong/TinyJs.git TinyJS
    * ServerNgx/build.sh release
* Install openresty
    * https://openresty.org/en/linux-packages.html
    * 安装插件
      * sudo opm get bungle/lua-resty-template
      * sudo opm get GUI/lua-resty-mail
      * opm get ledgetech/lua-resty-http
* 将 database 目录复制到 server: /mlserver/database
* 运行
  * deploy.sh
  * 配置 /mlserver/lua/conf.lua 中
  * 生成密码 https://us-west-2.console.aws.amazon.com/ses/home#/smtp
    _M.smtp_username = "",
    _M.smtp_password = "",

* 配置 openresty/nginx 
    * sudo vi /usr/local/openresty/nginx/conf/nginx.conf
        * 在 root 下添加
          pid        /usr/local/openresty/nginx/logs/nginx.pid;
        * 在 http 下添加
          include /mlserver/conf/mlserver.conf;
        * Optimize for performance: https://gist.github.com/denji/8359866
            worker_processes auto;
            worker_rlimit_nofile 100000;
            events {
                # determines how much clients will be served per worker
                # max clients = worker_connections * worker_processes
                # max clients is also limited by the number of socket connections available on the system (~64k)
                worker_connections 16384;
            }

            http {
                # cache informations about FDs, frequently accessed files
                # can boost performance, but you need to test those values
                open_file_cache max=3000 inactive=20s;
                open_file_cache_valid 30s;
                open_file_cache_min_uses 2;
                open_file_cache_errors on;
                
                # copies data between one FD and other from within the kernel
                # faster than read() + write()
                sendfile on;

                # send headers in one piece, it is better than sending them one by one
                tcp_nopush on;

                # don't buffer data sent, good for small data bursts in real time
                tcp_nodelay on;

                client_max_body_size 200k;

                # allow the server to close connection on non responding client, this will free up memory
                reset_timedout_connection on;

                # request timed out -- default 60
                client_body_timeout 20;

                # if client stop responding, free up memory -- default 60
                send_timeout 10;

                # server will close connection after this time -- default 75
                keepalive_timeout 10;
            }

      #第一次部署时不include /mlserver/conf/mlserver.conf，在 root server 下添加，并创建目录 /mlserver/www-static/.well-known
      location /.well-known {
          alias /mlserver/www-static/.well-known;
      }

* https 支持
    * https://certbot.eff.org/instructions?ws=nginx&os=pip&commit=%3E
      * sudo certbot certonly --webroot
        * 配置域名 viewlyrics.com,www.viewlyrics.com,en.viewlyrics.com,zh-cn.viewlyrics.com,crintsoft.com,www.crintsoft.com,en.crintsoft.com,zh-cn.crintsoft.com,v.crintsoft.com,minilyrics.com,www.minilyrics.com,en.minilyrics.com,zh-cn.minilyrics.com
        * 配置保存路径: /mlserver/www-static
    * Add crontab:
        sudo crontab -e
        0   1  1   *   *     /mlserver/bin/renew-https.sh > /var/log/renew_https.log 2>&1
        0   2  *   *   *     /mlserver/bin/commit-lyrics.sh > /var/log/commit-lyrics.log 2>&1

## 升级部署

* 代码放在 /home/ubuntu/Mp3Player 目录下
  * 取最新代码, 编译
    * 代码在 /home/ubuntu/Mp3Player 下
    * cd ./Mp3Player/LyrServer
    * ./build.sh release && sudo ./deploy.sh release
    * sudo openresty -s reload
    * systemctl start lyrics-server
    * /home/ubuntu/Mp3Player/ServerNgx/build.sh release && sudo /home/ubuntu/Mp3Player/ServerNgx/deploy.sh release