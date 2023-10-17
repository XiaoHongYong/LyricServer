//
//  AesUtil.cpp
//  LyricsServer
//
//  Created by henry_xiao on 2023/10/11.
//

#include "AesUtil.hpp"


#define FOOTER_LEN (sizeof(uint32_t) + sizeof(uint32_t))
#define CRC32_START 0xFFFFFFFF

void randArray(uint8_t *arr, size_t size) {
    for (size_t i = 0; i < size; i++) {
        arr[i] = rand() % 256;
    }
}

string uint32ToBE(uint32_t n) {
    uint8_t buf[8];
    uint32ToBE(n, buf);
    return string((char *)buf, 4);
}

string aesEncrypt(AES_ctx &ctx, const StringView &input) {
    randArray(ctx.Iv, AES_BLOCKLEN);

    string output;

    // iv: 16
    output.append((cstr_t)ctx.Iv, AES_BLOCKLEN);

    unsigned long crc = CRC32_START;
    crc = crc32(crc, (uint8_t *)input.data, input.len);

    output.append(input.data, input.len);

    int padding = AES_BLOCKLEN - ((input.len + FOOTER_LEN) % AES_BLOCKLEN);
    if (padding != AES_BLOCKLEN) {
        // padding to AES_BLOCKLEN
        uint8_t buf[AES_BLOCKLEN];
        randArray(buf, padding);

        // padding
        output.append((char *)buf, padding);
    }

    // length: 4
    output.append(uint32ToBE((uint32_t)input.len));

    // crc32: 4
    output.append(uint32ToBE((uint32_t)crc));

    AES_CBC_encrypt_buffer(&ctx, (uint8_t *)output.c_str() + AES_BLOCKLEN, output.size() - AES_BLOCKLEN);

    return output;
}

string aesDecrypt(AES_ctx &ctx, const StringView &input) {
    string output;

    if (input.len % AES_BLOCKLEN != 0 || input.len < AES_BLOCKLEN * 2) {
        DLOG(INFO) << "aesDecrypt, invalid input length: " << input.len;
        return output;
    }

    uint8_t *iv = (uint8_t *)input.data;
    AES_ctx_set_iv(&ctx, iv);

    // 去掉 iv
    output.assign(input.data + AES_BLOCKLEN, input.len - AES_BLOCKLEN);

    AES_CBC_decrypt_buffer(&ctx, (uint8_t *)output.data(), output.size());

    uint8_t *p = (uint8_t *)output.data() + output.size() - FOOTER_LEN;
    uint32_t len = uint32FromBE(p); p += sizeof(uint32_t);
    uint32_t crcExpected = uint32FromBE(p);

    if (len > output.size() - FOOTER_LEN) {
        DLOG(ERROR) << "Length is larger than received: " << len << ", received size: " << output.size() - FOOTER_LEN;
        return "";
    }

    output.resize(len);
    auto crc = (uint32_t)crc32(CRC32_START, (uint8_t *)output.data(), len);
    if (crcExpected != crc) {
        // crc 不正确
        DLOG(ERROR) << "Incorrect crc32, crc: " << crc << ", expected crc: " << crcExpected;
        return "";
    }

    return output;
}


#if UNIT_TEST

#include "utils/unittest.h"

TEST(AesUtil, crypt) {
    AES_ctx ctx;

    const char *KEY = "TNDGZYU8NT9HPNFS3B190ZSEOLUBOBFQ";

    AES_init_ctx(&ctx, (const uint8_t *)KEY);

    string data = "UCCTGOGML4Q7V1AK7CORUYZQVMPYU58G25UYU3L6UJZXJT3X2R8LUTVD4YIAK9NK";
    for (int i = 0; i < data.size(); i++) {
        printf("Round: %d\n", i);
        StringView in(data.data(), i);
        auto encrypted = aesEncrypt(ctx, in);
        auto out = aesDecrypt(ctx, encrypted);
        ASSERT_EQ(in.toString(), out);
    }
}

#endif
