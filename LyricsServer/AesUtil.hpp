//
//  AesUtil.hpp
//  LyricsServer
//
//  Created by henry_xiao on 2023/10/11.
//

#ifndef AesUtil_hpp
#define AesUtil_hpp

#include "aes.hpp"
#include "Types.h"


string aesEncrypt(AES_ctx &ctx, const StringView &input);
string aesDecrypt(AES_ctx &ctx, const StringView &input);


#endif /* AesUtil_hpp */
