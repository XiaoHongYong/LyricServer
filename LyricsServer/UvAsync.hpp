//
//  UvAsync.hpp
//  data-blink
//
//  Created by henry_xiao on 2023/5/26.
//

#pragma once

#ifndef UvAsync_hpp
#define UvAsync_hpp

#include "Types.h"
#include "uv.h"

using FunctionVoidP0 = std::function<void()>;

class UvAsync {
public:
    static void sendAsyncCallback(uv_loop_t *loop, FunctionVoidP0 callback) {
        UvAsync *async = new UvAsync(loop, callback);
        async->send();
    }

private:
    UvAsync(uv_loop_t *loop, FunctionVoidP0 callback) : _callback(callback) {
        _async.data = this;
        uv_async_init(loop, &_async, asyncCb);
    }

    void send() {
        uv_async_send(&_async);
    }

    static void asyncCb(uv_async_t *async) {
        UvAsync *thiz = (UvAsync *)async->data;
        thiz->_callback();

        uv_close((uv_handle_t*)async, closeCb);
    }

    static void closeCb(uv_handle_t *async) {
        UvAsync *thiz = (UvAsync *)async->data;
        delete thiz;
    }

    uv_async_t              _async;
    FunctionVoidP0          _callback;

};

#endif /* UvAsync_hpp */
