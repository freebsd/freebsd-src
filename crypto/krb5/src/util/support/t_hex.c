/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* util/support/t_hex.c - Test hex encoding and decoding */
/*
 * Copyright (C) 2018 by the Massachusetts Institute of Technology.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <k5-platform.h>
#include <k5-hex.h>

struct {
    const char *hex;
    const char *binary;
    size_t binary_len;
    int uppercase;
} tests[] = {
    /* Invalid hex strings */
    { "1" },
    { "123" },
    { "0/" },
    { "/0" },
    { "0:" },
    { ":0" },
    { "0@" },
    { "@0" },
    { "0G" },
    { "G0" },
    { "0`" },
    { "`0" },
    { "0g" },
    { "g0" },
    { " 00 " },
    { "0\x01" },

    { "", "", 0 },
    { "00", "\x00", 1 },
    { "01", "\x01", 1 },
    { "10", "\x10", 1 },
    { "01ff", "\x01\xFF", 2 },
    { "A0B0C0", "\xA0\xB0\xC0", 3, 1 },
    { "1a2b3c4d5e6f", "\x1A\x2B\x3C\x4D\x5E\x6F", 6 },
    { "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff",
      "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
      "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF", 32 },

    /* All byte values, lowercase */
    { "0001020304050607", "\x00\x01\x02\x03\x04\x05\x06\x07", 8 },
    { "08090a0b0c0d0e0f", "\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F", 8 },
    { "1011121314151617", "\x10\x11\x12\x13\x14\x15\x16\x17", 8 },
    { "18191a1b1c1d1e1f", "\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F", 8 },
    { "2021222324252627", "\x20\x21\x22\x23\x24\x25\x26\x27", 8 },
    { "28292a2b2c2d2e2f", "\x28\x29\x2A\x2B\x2C\x2D\x2E\x2F", 8 },
    { "3031323334353637", "\x30\x31\x32\x33\x34\x35\x36\x37", 8 },
    { "38393a3b3c3d3e3f", "\x38\x39\x3A\x3B\x3C\x3D\x3E\x3F", 8 },
    { "4041424344454647", "\x40\x41\x42\x43\x44\x45\x46\x47", 8 },
    { "48494a4b4c4d4e4f", "\x48\x49\x4A\x4B\x4C\x4D\x4E\x4F", 8 },
    { "5051525354555657", "\x50\x51\x52\x53\x54\x55\x56\x57", 8 },
    { "58595a5b5c5d5e5f", "\x58\x59\x5A\x5B\x5C\x5D\x5E\x5F", 8 },
    { "6061626364656667", "\x60\x61\x62\x63\x64\x65\x66\x67", 8 },
    { "68696a6b6c6d6e6f", "\x68\x69\x6A\x6B\x6C\x6D\x6E\x6F", 8 },
    { "7071727374757677", "\x70\x71\x72\x73\x74\x75\x76\x77", 8 },
    { "78797a7b7c7d7e7f", "\x78\x79\x7A\x7B\x7C\x7D\x7E\x7F", 8 },
    { "8081828384858687", "\x80\x81\x82\x83\x84\x85\x86\x87", 8 },
    { "88898a8b8c8d8e8f", "\x88\x89\x8A\x8B\x8C\x8D\x8E\x8F", 8 },
    { "9091929394959697", "\x90\x91\x92\x93\x94\x95\x96\x97", 8 },
    { "98999a9b9c9d9e9f", "\x98\x99\x9A\x9B\x9C\x9D\x9E\x9F", 8 },
    { "a0a1a2a3a4a5a6a7", "\xA0\xA1\xA2\xA3\xA4\xA5\xA6\xA7", 8 },
    { "a8a9aaabacadaeaf", "\xA8\xA9\xAA\xAB\xAC\xAD\xAE\xAF", 8 },
    { "b0b1b2b3b4b5b6b7", "\xB0\xB1\xB2\xB3\xB4\xB5\xB6\xB7", 8 },
    { "b8b9babbbcbdbebf", "\xB8\xB9\xBA\xBB\xBC\xBD\xBE\xBF", 8 },
    { "c0c1c2c3c4c5c6c7", "\xC0\xC1\xC2\xC3\xC4\xC5\xC6\xC7", 8 },
    { "c8c9cacbcccdcecf", "\xC8\xC9\xCA\xCB\xCC\xCD\xCE\xCF", 8 },
    { "d0d1d2d3d4d5d6d7", "\xD0\xD1\xD2\xD3\xD4\xD5\xD6\xD7", 8 },
    { "d8d9dadbdcdddedf", "\xD8\xD9\xDA\xDB\xDC\xDD\xDE\xDF", 8 },
    { "e0e1e2e3e4e5e6e7", "\xE0\xE1\xE2\xE3\xE4\xE5\xE6\xE7", 8 },
    { "e8e9eaebecedeeef", "\xE8\xE9\xEA\xEB\xEC\xED\xEE\xEF", 8 },
    { "f0f1f2f3f4f5f6f7", "\xF0\xF1\xF2\xF3\xF4\xF5\xF6\xF7", 8 },
    { "f8f9fafbfcfdfeff", "\xF8\xF9\xFA\xFB\xFC\xFD\xFE\xFF", 8 },

    /* All byte values, uppercase */
    { "0001020304050607", "\x00\x01\x02\x03\x04\x05\x06\x07", 8, 1 },
    { "08090A0B0C0D0E0F", "\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F", 8, 1 },
    { "1011121314151617", "\x10\x11\x12\x13\x14\x15\x16\x17", 8, 1 },
    { "18191A1B1C1D1E1F", "\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F", 8, 1 },
    { "2021222324252627", "\x20\x21\x22\x23\x24\x25\x26\x27", 8, 1 },
    { "28292A2B2C2D2E2F", "\x28\x29\x2A\x2B\x2C\x2D\x2E\x2F", 8, 1 },
    { "3031323334353637", "\x30\x31\x32\x33\x34\x35\x36\x37", 8, 1 },
    { "38393A3B3C3D3E3F", "\x38\x39\x3A\x3B\x3C\x3D\x3E\x3F", 8, 1 },
    { "4041424344454647", "\x40\x41\x42\x43\x44\x45\x46\x47", 8, 1 },
    { "48494A4B4C4D4E4F", "\x48\x49\x4A\x4B\x4C\x4D\x4E\x4F", 8, 1 },
    { "5051525354555657", "\x50\x51\x52\x53\x54\x55\x56\x57", 8, 1 },
    { "58595A5B5C5D5E5F", "\x58\x59\x5A\x5B\x5C\x5D\x5E\x5F", 8, 1 },
    { "6061626364656667", "\x60\x61\x62\x63\x64\x65\x66\x67", 8, 1 },
    { "68696A6B6C6D6E6F", "\x68\x69\x6A\x6B\x6C\x6D\x6E\x6F", 8, 1 },
    { "7071727374757677", "\x70\x71\x72\x73\x74\x75\x76\x77", 8, 1 },
    { "78797A7B7C7D7E7F", "\x78\x79\x7A\x7B\x7C\x7D\x7E\x7F", 8, 1 },
    { "8081828384858687", "\x80\x81\x82\x83\x84\x85\x86\x87", 8, 1 },
    { "88898A8B8C8D8E8F", "\x88\x89\x8A\x8B\x8C\x8D\x8E\x8F", 8, 1 },
    { "9091929394959697", "\x90\x91\x92\x93\x94\x95\x96\x97", 8, 1 },
    { "98999A9B9C9D9E9F", "\x98\x99\x9A\x9B\x9C\x9D\x9E\x9F", 8, 1 },
    { "A0A1A2A3A4A5A6A7", "\xA0\xA1\xA2\xA3\xA4\xA5\xA6\xA7", 8, 1 },
    { "A8A9AAABACADAEAF", "\xA8\xA9\xAA\xAB\xAC\xAD\xAE\xAF", 8, 1 },
    { "B0B1B2B3B4B5B6B7", "\xB0\xB1\xB2\xB3\xB4\xB5\xB6\xB7", 8, 1 },
    { "B8B9BABBBCBDBEBF", "\xB8\xB9\xBA\xBB\xBC\xBD\xBE\xBF", 8, 1 },
    { "C0C1C2C3C4C5C6C7", "\xC0\xC1\xC2\xC3\xC4\xC5\xC6\xC7", 8, 1 },
    { "C8C9CACBCCCDCECF", "\xC8\xC9\xCA\xCB\xCC\xCD\xCE\xCF", 8, 1 },
    { "D0D1D2D3D4D5D6D7", "\xD0\xD1\xD2\xD3\xD4\xD5\xD6\xD7", 8, 1 },
    { "D8D9DADBDCDDDEDF", "\xD8\xD9\xDA\xDB\xDC\xDD\xDE\xDF", 8, 1 },
    { "E0E1E2E3E4E5E6E7", "\xE0\xE1\xE2\xE3\xE4\xE5\xE6\xE7", 8, 1 },
    { "E8E9EAEBECEDEEEF", "\xE8\xE9\xEA\xEB\xEC\xED\xEE\xEF", 8, 1 },
    { "F0F1F2F3F4F5F6F7", "\xF0\xF1\xF2\xF3\xF4\xF5\xF6\xF7", 8, 1 },
    { "F8F9FAFBFCFDFEFF", "\xF8\xF9\xFA\xFB\xFC\xFD\xFE\xFF", 8, 1 },
};

int main()
{
    size_t i;
    char *hex;
    int ret;
    uint8_t *bytes;
    size_t len;

    for (i = 0; i < sizeof(tests) / sizeof(*tests); i++) {
        if (tests[i].binary == NULL) {
            ret = k5_hex_decode(tests[i].hex, &bytes, &len);
            assert(ret == EINVAL && bytes == NULL && len == 0);
            continue;
        }

        ret = k5_hex_decode(tests[i].hex, &bytes, &len);
        assert(ret == 0);
        assert(len == tests[i].binary_len);
        assert(memcmp(bytes, tests[i].binary, len) == 0);
        assert(bytes[len] == 0);
        free(bytes);

        ret = k5_hex_encode((uint8_t *)tests[i].binary, tests[i].binary_len,
                            tests[i].uppercase, &hex);
        assert(ret == 0);
        assert(strcmp(tests[i].hex, hex) == 0);
        free(hex);
    }
    return 0;
}
