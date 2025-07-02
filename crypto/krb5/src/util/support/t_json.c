/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* util/support/t_json.c - JSON test program */
/*
 * Copyright (c) 2010 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*
 * Copyright (C) 2012 by the Massachusetts Institute of Technology.
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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <k5-json.h>

static void
err(const char *str)
{
    fprintf(stderr, "%s\n", str);
    exit(1);
}

static void
check(int pred, const char *str)
{
    if (!pred)
        err(str);
}

static void
test_array()
{
    k5_json_string v1;
    k5_json_number v2;
    k5_json_null v3;
    k5_json_array a;
    k5_json_value v;

    k5_json_array_create(&a);
    k5_json_string_create("abc", &v1);
    k5_json_array_add(a, v1);
    k5_json_number_create(2, &v2);
    k5_json_array_add(a, v2);
    k5_json_null_create(&v3);
    k5_json_array_add(a, v3);

    check(k5_json_array_length(a) == 3, "array length");
    v = k5_json_array_get(a, 2);
    check(k5_json_get_tid(v) == K5_JSON_TID_NULL, "array[2] tid");
    v = k5_json_array_get(a, 1);
    check(k5_json_get_tid(v) == K5_JSON_TID_NUMBER, "array[1] tid");
    check(k5_json_number_value(v) == 2, "array[1] value");
    v = k5_json_array_get(a, 0);
    check(k5_json_get_tid(v) == K5_JSON_TID_STRING, "array[0] tid");
    check(strcmp(k5_json_string_utf8(v), "abc") == 0, "array[0] value");

    k5_json_release(v1);
    k5_json_release(v2);
    k5_json_release(a);

    k5_json_array_fmt(&a, "vnbiLssB", v3, 1, 9, (long long)-6, "def", NULL,
                      (void *)"ghij", (size_t)4);
    v = k5_json_array_get(a, 0);
    check(k5_json_get_tid(v) == K5_JSON_TID_NULL, "fmt array[0] tid");
    v = k5_json_array_get(a, 1);
    check(k5_json_get_tid(v) == K5_JSON_TID_NULL, "fmt array[1] tid");
    v = k5_json_array_get(a, 2);
    check(k5_json_get_tid(v) == K5_JSON_TID_BOOL, "fmt array[2] tid");
    check(k5_json_bool_value(v), "fmt array[2] value");
    v = k5_json_array_get(a, 3);
    check(k5_json_get_tid(v) == K5_JSON_TID_NUMBER, "fmt array[3] tid");
    check(k5_json_number_value(v) == 9, "fmt array[3] value");
    v = k5_json_array_get(a, 4);
    check(k5_json_get_tid(v) == K5_JSON_TID_NUMBER, "fmt array[4] tid");
    check(k5_json_number_value(v) == -6, "fmt array[4] value");
    v = k5_json_array_get(a, 5);
    check(k5_json_get_tid(v) == K5_JSON_TID_STRING, "fmt array[5] tid");
    check(strcmp(k5_json_string_utf8(v), "def") == 0, "fmt array[5] value");
    v = k5_json_array_get(a, 6);
    check(k5_json_get_tid(v) == K5_JSON_TID_NULL, "fmt array[6] tid");
    v = k5_json_array_get(a, 7);
    check(k5_json_get_tid(v) == K5_JSON_TID_STRING, "fmt array[7] tid");
    check(strcmp(k5_json_string_utf8(v), "Z2hpag==") == 0,
          "fmt array[7] value");
    k5_json_release(v3);
    k5_json_release(a);
}

static void
test_object(void)
{
    k5_json_object object;
    k5_json_number n, v1;
    k5_json_string s, v2;

    k5_json_object_create(&object);
    k5_json_number_create(1, &v1);
    k5_json_object_set(object, "key1", v1);
    k5_json_string_create("hejsan", &v2);
    k5_json_object_set(object, "key2", v2);

    n = k5_json_object_get(object, "key1");
    if (k5_json_number_value(n) != 1)
        err("Retrieving key1 from object failed");

    s = k5_json_object_get(object, "key2");
    if (strcmp(k5_json_string_utf8(s), "hejsan") != 0)
        err("Retrieving key2 from object failed");

    check(k5_json_object_get(object, "key3") == NULL,
          "object nonexistent key");

    k5_json_object_set(object, "key1", NULL);
    check(k5_json_object_get(object, "key1") == NULL,
          "object removed key");
    check(k5_json_object_get(object, "key2") != NULL,
          "object remaining key");

    k5_json_release(v1);
    k5_json_release(v2);
    k5_json_release(object);
}

static void
test_string(void)
{
    k5_json_string s1, s2, s3;
    unsigned char *data;
    size_t len;

    k5_json_string_create("hejsan", &s1);
    k5_json_string_create("hejsan", &s2);
    k5_json_string_create_base64("55555", 5, &s3);

    if (strcmp(k5_json_string_utf8(s1), k5_json_string_utf8(s2)) != 0)
        err("Identical strings are not identical");
    if (strcmp(k5_json_string_utf8(s3), "NTU1NTU=") != 0)
        err("base64 string has incorrect value");
    k5_json_string_unbase64(s3, &data, &len);
    if (len != 5 || memcmp(data, "55555", 5) != 0)
        err("base64 string doesn't decode to correct value");
    free(data);

    k5_json_release(s1);
    k5_json_release(s2);
    k5_json_release(s3);
}

static void
test_json(void)
{
    static char *tests[] = {
        "{\"k1\":\"s1\",\"k2\":\"s2\"}",
        "{\"k1\":[\"s1\",\"s2\",\"s3\"],\"k2\":\"s3\"}",
        "{\"k1\":{\"k2\":\"s1\",\"k3\":\"s2\",\"k4\":\"s3\"},\"k5\":\"s4\"}",
        "[\"v1\",\"v2\",[\"v3\",\"v4\",[\"v 5\",\" v 7 \"]],-123456789,"
        "null,true,false,123456789,\"\"]",
        "-1",
        "\"\\\\abc\\\"\\nde\\b\\r/\\ff\\tghi\\u0001\\u001F\"",
        "9223372036854775807",
        "-9223372036854775808",
        NULL
    };
    char **tptr, *s, *enc, *p, orig;
    int i;
    k5_json_value v, v2;

    check(k5_json_decode("\"string\"", &v) == 0, "string1");
    check(k5_json_get_tid(v) == K5_JSON_TID_STRING, "string1 tid");
    check(strcmp(k5_json_string_utf8(v), "string") == 0, "string1 utf8");
    k5_json_release(v);

    check(k5_json_decode("\t \"foo\\\"bar\" ", &v) == 0, "string2");
    check(k5_json_get_tid(v) == K5_JSON_TID_STRING, "string2 tid");
    check(strcmp(k5_json_string_utf8(v), "foo\"bar") == 0, "string2 utf8");
    k5_json_release(v);

    check(k5_json_decode(" { \"key\" : \"value\" }", &v) == 0, "object1");
    check(k5_json_get_tid(v) == K5_JSON_TID_OBJECT, "object1 tid");
    v2 = k5_json_object_get(v, "key");
    check(v2 != NULL, "object[key]");
    check(k5_json_get_tid(v2) == K5_JSON_TID_STRING, "object1[key] tid");
    check(strcmp(k5_json_string_utf8(v2), "value") == 0, "object1[key] utf8");
    k5_json_release(v);

    check(k5_json_decode("{ \"k1\" : { \"k2\" : \"s2\", \"k3\" : \"s3\" }, "
                         "\"k4\" : \"s4\" }", &v) == 0, "object2");
    v2 = k5_json_object_get(v, "k1");
    check(v2 != NULL, "object2[k1]");
    check(k5_json_get_tid(v2) == K5_JSON_TID_OBJECT, "object2[k1] tid");
    v2 = k5_json_object_get(v2, "k3");
    check(v2 != NULL, "object2[k1][k3]");
    check(k5_json_get_tid(v2) == K5_JSON_TID_STRING, "object2[k1][k3] tid");
    check(strcmp(k5_json_string_utf8(v2), "s3") == 0, "object2[k1][k3] utf8");
    k5_json_release(v);

    check(k5_json_decode("{ \"k1\" : 1 }", &v) == 0, "object3");
    check(k5_json_get_tid(v) == K5_JSON_TID_OBJECT, "object3 id");
    v2 = k5_json_object_get(v, "k1");
    check(k5_json_get_tid(v2) == K5_JSON_TID_NUMBER, "object3[k1] tid");
    check(k5_json_number_value(v2) == 1, "object3[k1] value");
    k5_json_release(v);

    check(k5_json_decode("-10", &v) == 0, "number1");
    check(k5_json_get_tid(v) == K5_JSON_TID_NUMBER, "number1 tid");
    check(k5_json_number_value(v) == -10, "number1 value");
    k5_json_release(v);

    check(k5_json_decode("99", &v) == 0, "number2");
    check(k5_json_get_tid(v) == K5_JSON_TID_NUMBER, "number2 tid");
    check(k5_json_number_value(v) == 99, "number2 value");
    k5_json_release(v);

    check(k5_json_decode(" [ 1 ]", &v) == 0, "array1");
    check(k5_json_get_tid(v) == K5_JSON_TID_ARRAY, "array1 tid");
    check(k5_json_array_length(v) == 1, "array1 len");
    v2 = k5_json_array_get(v, 0);
    check(v2 != NULL, "array1[0]");
    check(k5_json_get_tid(v2) == K5_JSON_TID_NUMBER, "array1[0] tid");
    check(k5_json_number_value(v2) == 1, "array1[0] value");
    k5_json_release(v);

    check(k5_json_decode(" [ -1 ]", &v) == 0, "array2");
    check(k5_json_get_tid(v) == K5_JSON_TID_ARRAY, "array2 tid");
    check(k5_json_array_length(v) == 1, "array2 len");
    v2 = k5_json_array_get(v, 0);
    check(v2 != NULL, "array2[0]");
    check(k5_json_get_tid(v2) == K5_JSON_TID_NUMBER, "array2[0] tid");
    check(k5_json_number_value(v2) == -1, "array2[0] value");
    k5_json_release(v);

    check(k5_json_decode("18446744073709551616", &v) == EOVERFLOW,
          "unsigned 64-bit overflow");
    check(k5_json_decode("9223372036854775808", &v) == EOVERFLOW,
          "signed 64-bit positive overflow");
    check(k5_json_decode("-9223372036854775809", &v) == EOVERFLOW,
          "signed 64-bit negative overflow");

    for (tptr = tests; *tptr != NULL; tptr++) {
        s = strdup(*tptr);
        if (k5_json_decode(s, &v))
            err(s);
        if (k5_json_encode(v, &enc) || strcmp(enc, s) != 0)
            err(s);
        free(enc);
        k5_json_release(v);

        /* Fuzz bytes.  Parsing may succeed or fail; we're just looking for
         * memory access bugs. */
        for (p = s; *p != '\0'; p++) {
            orig = *p;
            for (i = 0; i <= 255; i++) {
                *p = i;
                k5_json_decode(s, &v);
                k5_json_release(v);
            }
            *p = orig;
        }
        free(s);
    }
}

int
main(int argc, char **argv)
{
    test_array();
    test_object();
    test_string();
    test_json();
    return 0;
}
