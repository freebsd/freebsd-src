/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* util/support/t_k5buf.c - Test the k5buf string buffer module */
/*
 * Copyright 2008 Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

#include "k5-platform.h"
#include "k5-buf.h"
#include <stdio.h>
#include <stdlib.h>

static void
fail_if(int condition, const char *name)
{
    if (condition) {
        fprintf(stderr, "%s failed\n", name);
        exit(1);
    }
}

/* Test the invariants of a buffer. */
static void
check_buf(struct k5buf *buf, const char *name)
{
    fail_if(buf->buftype != K5BUF_FIXED && buf->buftype != K5BUF_DYNAMIC &&
            buf->buftype != K5BUF_ERROR, name);
    if (buf->buftype == K5BUF_ERROR) {
        fail_if(buf->data != NULL, name);
        fail_if(buf->space != 0 || buf->len != 0, name);
    } else {
        fail_if(buf->space == 0, name);
        fail_if(buf->len >= buf->space, name);
    }
}

static void
test_basic()
{
    struct k5buf buf;
    char storage[1024];

    k5_buf_init_fixed(&buf, storage, sizeof(storage));
    k5_buf_add(&buf, "Hello ");
    k5_buf_add_len(&buf, "world", 5);
    check_buf(&buf, "basic fixed");
    fail_if(buf.data == NULL || buf.len != 11, "basic fixed");
    fail_if(memcmp(buf.data, "Hello world", 11) != 0, "basic fixed");

    k5_buf_init_dynamic(&buf);
    k5_buf_add_len(&buf, "Hello", 5);
    k5_buf_add(&buf, " world");
    check_buf(&buf, "basic dynamic");
    fail_if(buf.data == NULL || buf.len != 11, "basic dynamic");
    fail_if(memcmp(buf.data, "Hello world", 11) != 0, "basic dynamic");
    k5_buf_free(&buf);
}

static void
test_realloc()
{
    struct k5buf buf;
    char data[1024];
    size_t i;

    for (i = 0; i < sizeof(data); i++)
        data[i] = 'a';

    /* Cause the buffer size to double from 128 to 256 bytes. */
    k5_buf_init_dynamic(&buf);
    k5_buf_add_len(&buf, data, 10);
    k5_buf_add_len(&buf, data, 128);
    fail_if(buf.space != 256, "realloc 1");
    check_buf(&buf, "realloc 1");
    fail_if(buf.data == NULL || buf.len != 138, "realloc 1");
    fail_if(memcmp(buf.data, data, buf.len) != 0, "realloc 1");

    /* Cause the same buffer to double in size to 512 bytes. */
    k5_buf_add_len(&buf, data, 128);
    fail_if(buf.space != 512, "realloc 2");
    check_buf(&buf, "realloc 2");
    fail_if(buf.data == NULL || buf.len != 266, "realloc 2");
    fail_if(memcmp(buf.data, data, buf.len) != 0, "realloc 2");
    k5_buf_free(&buf);

    /* Cause a buffer to increase from 128 to 512 bytes directly. */
    k5_buf_init_dynamic(&buf);
    k5_buf_add_len(&buf, data, 10);
    k5_buf_add_len(&buf, data, 256);
    fail_if(buf.space != 512, "realloc 3");
    check_buf(&buf, "realloc 3");
    fail_if(buf.data == NULL || buf.len != 266, "realloc 3");
    fail_if(memcmp(buf.data, data, buf.len) != 0, "realloc 3");
    k5_buf_free(&buf);

    /* Cause a buffer to increase from 128 to 1024 bytes directly. */
    k5_buf_init_dynamic(&buf);
    k5_buf_add_len(&buf, data, 10);
    k5_buf_add_len(&buf, data, 512);
    fail_if(buf.space != 1024, "realloc 4");
    check_buf(&buf, "realloc 4");
    fail_if(buf.data == NULL || buf.len != 522, "realloc 4");
    fail_if(memcmp(buf.data, data, buf.len) != 0, "realloc 4");
    k5_buf_free(&buf);

    /* Cause a reallocation to fail by integer overflow. */
    k5_buf_init_dynamic(&buf);
    k5_buf_add_len(&buf, data, 100);
    k5_buf_add_len(&buf, NULL, SIZE_MAX);
    check_buf(&buf, "realloc 5");
    fail_if(buf.buftype != K5BUF_ERROR, "realloc 5");
    k5_buf_free(&buf);
}

static void
test_overflow()
{
    struct k5buf buf;
    char storage[10];

    /* Cause a fixed-sized buffer overflow. */
    k5_buf_init_fixed(&buf, storage, sizeof(storage));
    k5_buf_add(&buf, "12345");
    k5_buf_add(&buf, "123456");
    check_buf(&buf, "overflow 1");
    fail_if(buf.buftype != K5BUF_ERROR, "overflow 1");

    /* Cause a fixed-sized buffer overflow with integer overflow. */
    k5_buf_init_fixed(&buf, storage, sizeof(storage));
    k5_buf_add(&buf, "12345");
    k5_buf_add_len(&buf, NULL, SIZE_MAX);
    check_buf(&buf, "overflow 2");
    fail_if(buf.buftype != K5BUF_ERROR, "overflow 2");
}

static void
test_error()
{
    struct k5buf buf;
    char storage[1];

    /* Cause an overflow and then perform actions afterwards. */
    k5_buf_init_fixed(&buf, storage, sizeof(storage));
    k5_buf_add(&buf, "12");
    fail_if(buf.buftype != K5BUF_ERROR, "error");
    check_buf(&buf, "error");
    k5_buf_add(&buf, "test");
    check_buf(&buf, "error");
    k5_buf_add_len(&buf, "test", 4);
    check_buf(&buf, "error");
    k5_buf_truncate(&buf, 3);
    check_buf(&buf, "error");
    fail_if(buf.buftype != K5BUF_ERROR, "error");
}

static void
test_truncate()
{
    struct k5buf buf;

    k5_buf_init_dynamic(&buf);
    k5_buf_add(&buf, "abcde");
    k5_buf_add(&buf, "fghij");
    k5_buf_truncate(&buf, 7);
    check_buf(&buf, "truncate");
    fail_if(buf.data == NULL || buf.len != 7, "truncate");
    fail_if(memcmp(buf.data, "abcdefg", 7) != 0, "truncate");
    k5_buf_free(&buf);
}

static void
test_binary()
{
    struct k5buf buf;
    char data[] = { 'a', 0, 'b' }, *s;

    k5_buf_init_dynamic(&buf);
    k5_buf_add_len(&buf, data, 3);
    k5_buf_add_len(&buf, data, 3);
    check_buf(&buf, "binary");
    fail_if(buf.data == NULL || buf.len != 6, "binary");
    s = buf.data;
    fail_if(s[0] != 'a' || s[1] != 0 || s[2] != 'b', "binary");
    fail_if(s[3] != 'a' || s[4] != 0 || s[5] != 'b', "binary");
    k5_buf_free(&buf);
}

static void
test_fmt()
{
    struct k5buf buf;
    char storage[10], data[1024];
    size_t i;

    for (i = 0; i < sizeof(data) - 1; i++)
        data[i] = 'a';
    data[i] = '\0';

    /* Format some text into a non-empty fixed buffer. */
    k5_buf_init_fixed(&buf, storage, sizeof(storage));
    k5_buf_add(&buf, "foo");
    k5_buf_add_fmt(&buf, " %d ", 3);
    check_buf(&buf, "fmt 1");
    fail_if(buf.data == NULL || buf.len != 6, "fmt 1");
    fail_if(memcmp(buf.data, "foo 3 ", 6) != 0, "fmt 1");

    /* Overflow the same buffer with formatted text. */
    k5_buf_add_fmt(&buf, "%d%d%d%d", 1, 2, 3, 4);
    check_buf(&buf, "fmt 2");
    fail_if(buf.buftype != K5BUF_ERROR, "fmt 2");

    /* Format some text into a non-empty dynamic buffer. */
    k5_buf_init_dynamic(&buf);
    k5_buf_add(&buf, "foo");
    k5_buf_add_fmt(&buf, " %d ", 3);
    check_buf(&buf, "fmt 3");
    fail_if(buf.data == NULL || buf.len != 6, "fmt 3");
    fail_if(memcmp(buf.data, "foo 3 ", 6) != 0, "fmt 3");

    /* Format more text into the same buffer, causing a big resize. */
    k5_buf_add_fmt(&buf, "%s", data);
    check_buf(&buf, "fmt 4");
    fail_if(buf.space != 2048, "fmt 4");
    fail_if(buf.data == NULL || buf.len != 1029, "fmt 4");
    fail_if(memcmp((char *)buf.data + 6, data, 1023) != 0, "fmt 4");
    k5_buf_free(&buf);
}

int
main()
{
    test_basic();
    test_realloc();
    test_overflow();
    test_error();
    test_truncate();
    test_binary();
    test_fmt();
    return 0;
}
