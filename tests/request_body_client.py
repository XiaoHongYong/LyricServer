# encoding=utf-8

import socket
import sys
import time

from urlparse import urlparse

def send_parts(s, data, part_sizes, sleep_duration):
    offset = 0
    length = len(data)
    total = 0

    for size in part_sizes:
        size = min(size, length - offset)
        total += size

        print('Send:', total, size)
        s.sendall(data[offset:offset + size])
        offset += size
        if offset < length:
            print('Sleep:', sleep_duration)
            time.sleep(sleep_duration)


'''
# Request Body 测试场景

* 符合块分组格式
  * 数据从 1 ~ n 个 buffer_size
  * 数据只有头部的情况
  * 数据 Block 从 0 byte 到 86 * 86 - 1的情况
  * 数据跨 block
  * 每 decode 一个 block，则可以发送到 upstream；
  * 格式不正确的情况：
    * HEADER 格式正确，但是小于头大小

* 不符合块分组格式
  * 数据超过 16KB，则数据需要缓存到磁盘：适用于 Reload 的情况；
  * 数据从 1 ~ n 个 buffer_size
  * 数据可以直接发送到 Upstream，而不用额外的缓存
'''
def test_connect(length=0):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

    sleep_duration = 1
    if length == 0:
        length = 1024 * 1024 * 10
    part_sizes = [10, 10, 10, 10, 3, 5, 50, 100, 100, 100, 100, 1024 * 1, 1024 * 10]
    part_sizes.append(length)

    s.connect(("localhost", 8000))
    header = "POST / HTTP/1.1\r\nHost: localhost:8000\r\nConnection: Keep-Alive\r\nContent-Length: {}\r\nCookie: Xyz=Token\r\n\r\n".format(length)

    send_parts(s, header, part_sizes, sleep_duration)

    data = '*' * length

    send_parts(s, data, part_sizes, sleep_duration)

    #s.sendall(data)

    print('Receiving data...')
    while True:
        data = s.recv(1024 * 300)
        if not data:
            break
        print(data)
        print(len(data))
    s.close

def to_length(s):
    base = 1
    s = s.lower()
    if s.endswith('k'):
        base = 1024
        s = s[:-1]
    elif s.endswith('m'):
        base = 1024 * 1024
        s = s[:-1]
    else:
        base = 1
    return int(s) * base

def get_url(url, fn):
    r = urlparse(url)
    assert(r.scheme.lower() == 'http')
    host, _, _ = r.netloc.partition(':')
    port = r.port or 80

    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((host, port))
    header = "GET / HTTP/1.1\r\nHost: {}:{}\r\nAccept-Encoding: gzip, deflate\r\nConnection: close\r\n\r\n".format(host, port)
    print header

    s.sendall(header)

    body = ''
    print 'Receiving data...'
    while True:
        data = s.recv(1024 * 300)
        if not data:
            break
        body += data
        print len(data)
    s.close

    _, _, body = body.partition('\r\n\r\n')
    with open(fn, 'wb') as fp:
        fp.write(body)

if __name__ == '__main__':
    if len(sys.argv) > 1:
        test_connect(to_length(sys.argv[1]))

