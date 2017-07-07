/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* util/support/json.c - JSON parser and unparser */
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

/*
 * This file implements a minimal dynamic type system for JSON values and a
 * JSON encoder and decoder.  It is loosely based on the heimbase code from
 * Heimdal.
 */

#include <k5-platform.h>
#include <k5-base64.h>
#include <k5-json.h>
#include <k5-buf.h>

#define MAX_DECODE_DEPTH 64
#define MIN_ALLOC_SLOT   16

typedef void (*type_dealloc_fn)(void *val);

typedef struct json_type_st {
    k5_json_tid tid;
    const char *name;
    type_dealloc_fn dealloc;
} *json_type;

struct value_base {
    json_type isa;
    unsigned int ref_cnt;
};

#define PTR2BASE(ptr) (((struct value_base *)ptr) - 1)
#define BASE2PTR(ptr) ((void *)(((struct value_base *)ptr) + 1))

k5_json_value
k5_json_retain(k5_json_value val)
{
    struct value_base *p;

    if (val == NULL)
        return val;
    p = PTR2BASE(val);
    assert(p->ref_cnt != 0);
    p->ref_cnt++;
    return val;
}

void
k5_json_release(k5_json_value val)
{
    struct value_base *p;

    if (val == NULL)
        return;
    p = PTR2BASE(val);
    assert(p->ref_cnt != 0);
    p->ref_cnt--;
    if (p->ref_cnt == 0) {
        if (p->isa->dealloc != NULL)
            p->isa->dealloc(val);
        free(p);
    }
}

/* Get the type description of a k5_json_value. */
static json_type
get_isa(k5_json_value val)
{
    struct value_base *p = PTR2BASE(val);

    return p->isa;
}

k5_json_tid
k5_json_get_tid(k5_json_value val)
{
    json_type isa = get_isa(val);

    return isa->tid;
}

static k5_json_value
alloc_value(json_type type, size_t size)
{
    struct value_base *p = calloc(1, size + sizeof(*p));

    if (p == NULL)
        return NULL;
    p->isa = type;
    p->ref_cnt = 1;

    return BASE2PTR(p);
}

/*** Null type ***/

static struct json_type_st null_type = { K5_JSON_TID_NULL, "null", NULL };

int
k5_json_null_create(k5_json_null *val_out)
{
    *val_out = alloc_value(&null_type, 0);
    return (*val_out == NULL) ? ENOMEM : 0;
}

int
k5_json_null_create_val(k5_json_value *val_out)
{
    *val_out = alloc_value(&null_type, 0);
    return (*val_out == NULL) ? ENOMEM : 0;
}

/*** Boolean type ***/

static struct json_type_st bool_type = { K5_JSON_TID_BOOL, "bool", NULL };

int
k5_json_bool_create(int truth, k5_json_bool *val_out)
{
    k5_json_bool b;

    *val_out = NULL;
    b = alloc_value(&bool_type, 1);
    if (b == NULL)
        return ENOMEM;
    *(unsigned char *)b = !!truth;
    *val_out = b;
    return 0;
}

int
k5_json_bool_value(k5_json_bool bval)
{
    return *(unsigned char *)bval;
}

/*** Array type ***/

struct k5_json_array_st {
    k5_json_value *values;
    size_t len;
    size_t allocated;
};

static void
array_dealloc(void *ptr)
{
    k5_json_array array = ptr;
    size_t i;

    for (i = 0; i < array->len; i++)
        k5_json_release(array->values[i]);
    free(array->values);
}

static struct json_type_st array_type = {
    K5_JSON_TID_ARRAY, "array", array_dealloc
};

int
k5_json_array_create(k5_json_array *val_out)
{
    *val_out = alloc_value(&array_type, sizeof(struct k5_json_array_st));
    return (*val_out == NULL) ? ENOMEM : 0;
}

int
k5_json_array_add(k5_json_array array, k5_json_value val)
{
    k5_json_value *ptr;
    size_t new_alloc;

    if (array->len >= array->allocated) {
        /* Increase the number of slots by 50% (MIN_ALLOC_SLOT minimum). */
        new_alloc = array->len + 1 + (array->len >> 1);
        if (new_alloc < MIN_ALLOC_SLOT)
            new_alloc = MIN_ALLOC_SLOT;
        ptr = realloc(array->values, new_alloc * sizeof(*array->values));
        if (ptr == NULL)
            return ENOMEM;
        array->values = ptr;
        array->allocated = new_alloc;
    }
    array->values[array->len++] = k5_json_retain(val);
    return 0;
}

size_t
k5_json_array_length(k5_json_array array)
{
    return array->len;
}

k5_json_value
k5_json_array_get(k5_json_array array, size_t idx)
{
    if (idx >= array->len)
        abort();
    return array->values[idx];
}

void
k5_json_array_set(k5_json_array array, size_t idx, k5_json_value val)
{
    if (idx >= array->len)
        abort();
    k5_json_release(array->values[idx]);
    array->values[idx] = k5_json_retain(val);
}

int
k5_json_array_fmt(k5_json_array *array_out, const char *template, ...)
{
    const char *p;
    va_list ap;
    const char *cstring;
    unsigned char *data;
    size_t len;
    long long nval;
    k5_json_array array;
    k5_json_value val;
    k5_json_number num;
    k5_json_string str;
    k5_json_bool b;
    k5_json_null null;
    int truth, ret;

    *array_out = NULL;
    if (k5_json_array_create(&array))
        return ENOMEM;
    va_start(ap, template);
    for (p = template; *p != '\0'; p++) {
        switch (*p) {
        case 'v':
            val = k5_json_retain(va_arg(ap, k5_json_value));
            break;
        case 'n':
            if (k5_json_null_create(&null))
                goto err;
            val = null;
            break;
        case 'b':
            truth = va_arg(ap, int);
            if (k5_json_bool_create(truth, &b))
                goto err;
            val = b;
            break;
        case 'i':
            nval = va_arg(ap, int);
            if (k5_json_number_create(nval, &num))
                goto err;
            val = num;
            break;
        case 'L':
            nval = va_arg(ap, long long);
            if (k5_json_number_create(nval, &num))
                goto err;
            val = num;
            break;
        case 's':
            cstring = va_arg(ap, const char *);
            if (cstring == NULL) {
                if (k5_json_null_create(&null))
                    goto err;
                val = null;
            } else {
                if (k5_json_string_create(cstring, &str))
                    goto err;
                val = str;
            }
            break;
        case 'B':
            data = va_arg(ap, unsigned char *);
            len = va_arg(ap, size_t);
            if (k5_json_string_create_base64(data, len, &str))
                goto err;
            val = str;
            break;
        default:
            goto err;
        }
        ret = k5_json_array_add(array, val);
        k5_json_release(val);
        if (ret)
            goto err;
    }
    va_end(ap);
    *array_out = array;
    return 0;

err:
    va_end(ap);
    k5_json_release(array);
    return ENOMEM;
}

/*** Object type (string:value mapping) ***/

struct entry {
    char *key;
    k5_json_value value;
};

struct k5_json_object_st {
    struct entry *entries;
    size_t len;
    size_t allocated;
};

static void
object_dealloc(void *ptr)
{
    k5_json_object obj = ptr;
    size_t i;

    for (i = 0; i < obj->len; i++) {
        free(obj->entries[i].key);
        k5_json_release(obj->entries[i].value);
    }
    free(obj->entries);
}

static struct json_type_st object_type = {
    K5_JSON_TID_OBJECT, "object", object_dealloc
};

int
k5_json_object_create(k5_json_object *val_out)
{
    *val_out = alloc_value(&object_type, sizeof(struct k5_json_object_st));
    return (*val_out == NULL) ? ENOMEM : 0;
}

size_t
k5_json_object_count(k5_json_object obj)
{
    return obj->len;
}

/* Return the entry for key within obj, or NULL if none exists. */
static struct entry *
object_search(k5_json_object obj, const char *key)
{
    size_t i;

    for (i = 0; i < obj->len; i++) {
        if (strcmp(key, obj->entries[i].key) == 0)
            return &obj->entries[i];
    }
    return NULL;
}

k5_json_value
k5_json_object_get(k5_json_object obj, const char *key)
{
    struct entry *ent;

    ent = object_search(obj, key);
    if (ent == NULL)
        return NULL;
    return ent->value;
}

int
k5_json_object_set(k5_json_object obj, const char *key, k5_json_value val)
{
    struct entry *ent, *ptr;
    size_t new_alloc, i;

    ent = object_search(obj, key);
    if (ent != NULL) {
        k5_json_release(ent->value);
        if (val == NULL) {
            /* Remove this key. */
            free(ent->key);
            for (i = ent - obj->entries; i < obj->len - 1; i++)
                obj->entries[i] = obj->entries[i + 1];
            obj->len--;
        } else {
            /* Overwrite this key's value with the new one. */
            ent->value = k5_json_retain(val);
        }
        return 0;
    }

    /* If didn't find a key the caller asked to remove, do nothing. */
    if (val == NULL)
        return 0;

    if (obj->len >= obj->allocated) {
        /* Increase the number of slots by 50% (MIN_ALLOC_SLOT minimum). */
        new_alloc = obj->len + 1 + (obj->len >> 1);
        if (new_alloc < MIN_ALLOC_SLOT)
            new_alloc = MIN_ALLOC_SLOT;
        ptr = realloc(obj->entries, new_alloc * sizeof(*obj->entries));
        if (ptr == NULL)
            return ENOMEM;
        obj->entries = ptr;
        obj->allocated = new_alloc;
    }
    obj->entries[obj->len].key = strdup(key);
    if (obj->entries[obj->len].key == NULL)
        return ENOMEM;
    obj->entries[obj->len].value = k5_json_retain(val);
    obj->len++;
    return 0;
}

void
k5_json_object_iterate(k5_json_object obj, k5_json_object_iterator_fn func,
                       void *arg)
{
    size_t i;

    for (i = 0; i < obj->len; i++)
        func(arg, obj->entries[i].key, obj->entries[i].value);
}

/*** String type ***/

static struct json_type_st string_type = {
    K5_JSON_TID_STRING, "string", NULL
};

int
k5_json_string_create(const char *cstring, k5_json_string *val_out)
{
    return k5_json_string_create_len(cstring, strlen(cstring), val_out);
}

int
k5_json_string_create_len(const void *data, size_t len,
                          k5_json_string *val_out)
{
    char *s;

    *val_out = NULL;
    s = alloc_value(&string_type, len + 1);
    if (s == NULL)
        return ENOMEM;
    if (len > 0)
        memcpy(s, data, len);
    s[len] = '\0';
    *val_out = (k5_json_string)s;
    return 0;
}

int
k5_json_string_create_base64(const void *data, size_t len,
                             k5_json_string *val_out)
{
    char *base64;
    int ret;

    *val_out = NULL;
    base64 = k5_base64_encode(data, len);
    if (base64 == NULL)
        return ENOMEM;
    ret = k5_json_string_create(base64, val_out);
    free(base64);
    return ret;
}

const char *
k5_json_string_utf8(k5_json_string string)
{
    return (const char *)string;
}

int
k5_json_string_unbase64(k5_json_string string, unsigned char **data_out,
                        size_t *len_out)
{
    unsigned char *data;
    size_t len;

    *data_out = NULL;
    *len_out = 0;
    data = k5_base64_decode((const char *)string, &len);
    if (data == NULL)
        return (len == 0) ? ENOMEM : EINVAL;
    *data_out = data;
    *len_out = len;
    return 0;
}

/*** Number type ***/

static struct json_type_st number_type = {
    K5_JSON_TID_NUMBER, "number", NULL
};

int
k5_json_number_create(long long number, k5_json_number *val_out)
{
    k5_json_number n;

    *val_out = NULL;
    n = alloc_value(&number_type, sizeof(long long));
    if (n == NULL)
        return ENOMEM;
    *((long long *)n) = number;
    *val_out = n;
    return 0;
}

long long
k5_json_number_value(k5_json_number number)
{
    return *(long long *)number;
}

/*** JSON encoding ***/

static const char quotemap_json[] = "\"\\/bfnrt";
static const char quotemap_c[] = "\"\\/\b\f\n\r\t";
static const char needs_quote[] = "\\\"\1\2\3\4\5\6\7\10\11\12\13\14\15\16\17"
    "\20\21\22\23\24\25\26\27\30\31\32\33\34\35\36\37";

static int encode_value(struct k5buf *buf, k5_json_value val);

static void
encode_string(struct k5buf *buf, const char *str)
{
    size_t n;
    const char *p;

    k5_buf_add(buf, "\"");
    while (*str != '\0') {
        n = strcspn(str, needs_quote);
        k5_buf_add_len(buf, str, n);
        str += n;
        if (*str == '\0')
            break;
        k5_buf_add(buf, "\\");
        p = strchr(quotemap_c, *str);
        if (p != NULL)
            k5_buf_add_len(buf, quotemap_json + (p - quotemap_c), 1);
        else
            k5_buf_add_fmt(buf, "u00%02X", (unsigned int)*str);
        str++;
    }
    k5_buf_add(buf, "\"");
}

struct obj_ctx {
    struct k5buf *buf;
    int ret;
    int first;
};

static void
encode_obj_entry(void *ctx, const char *key, k5_json_value value)
{
    struct obj_ctx *j = ctx;

    if (j->ret)
        return;
    if (j->first)
        j->first = 0;
    else
        k5_buf_add(j->buf, ",");
    encode_string(j->buf, key);
    k5_buf_add(j->buf, ":");
    j->ret = encode_value(j->buf, value);
}

static int
encode_value(struct k5buf *buf, k5_json_value val)
{
    k5_json_tid type;
    int ret;
    size_t i, len;
    struct obj_ctx ctx;

    if (val == NULL)
        return EINVAL;

    type = k5_json_get_tid(val);
    switch (type) {
    case K5_JSON_TID_ARRAY:
        k5_buf_add(buf, "[");
        len = k5_json_array_length(val);
        for (i = 0; i < len; i++) {
            if (i != 0)
                k5_buf_add(buf, ",");
            ret = encode_value(buf, k5_json_array_get(val, i));
            if (ret)
                return ret;
        }
        k5_buf_add(buf, "]");
        return 0;

    case K5_JSON_TID_OBJECT:
        k5_buf_add(buf, "{");
        ctx.buf = buf;
        ctx.ret = 0;
        ctx.first = 1;
        k5_json_object_iterate(val, encode_obj_entry, &ctx);
        k5_buf_add(buf, "}");
        return ctx.ret;

    case K5_JSON_TID_STRING:
        encode_string(buf, k5_json_string_utf8(val));
        return 0;

    case K5_JSON_TID_NUMBER:
        k5_buf_add_fmt(buf, "%lld", k5_json_number_value(val));
        return 0;

    case K5_JSON_TID_NULL:
        k5_buf_add(buf, "null");
        return 0;

    case K5_JSON_TID_BOOL:
        k5_buf_add(buf, k5_json_bool_value(val) ? "true" : "false");
        return 0;

    default:
        return EINVAL;
    }
}

int
k5_json_encode(k5_json_value val, char **json_out)
{
    struct k5buf buf;
    int ret;

    *json_out = NULL;
    k5_buf_init_dynamic(&buf);
    ret = encode_value(&buf, val);
    if (ret) {
        k5_buf_free(&buf);
        return ret;
    }
    if (k5_buf_status(&buf) != 0)
        return ENOMEM;
    *json_out = buf.data;
    return 0;
}

/*** JSON decoding ***/

struct decode_ctx {
    const unsigned char *p;
    size_t depth;
};

static int parse_value(struct decode_ctx *ctx, k5_json_value *val_out);

/* Consume whitespace.  Return 0 if there is anything left to parse after the
 * whitespace, -1 if not. */
static int
white_spaces(struct decode_ctx *ctx)
{
    unsigned char c;

    for (; *ctx->p != '\0'; ctx->p++) {
        c = *ctx->p;
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n')
            return 0;
    }
    return -1;
}

/* Return true if c is a decimal digit. */
static inline int
is_digit(unsigned char c)
{
    return ('0' <= c && c <= '9');
}

/* Return true if c is a hexadecimal digit (per RFC 5234 HEXDIG). */
static inline int
is_hex_digit(unsigned char c)
{
    return is_digit(c) || ('A' <= c && c <= 'F');
}

/* Return the numeric value of a hex digit; aborts if c is not a hex digit. */
static inline unsigned int
hexval(unsigned char c)
{
    if (is_digit(c))
        return c - '0';
    else if ('A' <= c && c <= 'F')
        return c - 'A' + 10;
    abort();
}

/* Parse a JSON number (which must be an integer in the signed 64-bit range; we
 * do not allow floating-point numbers). */
static int
parse_number(struct decode_ctx *ctx, k5_json_number *val_out)
{
    const unsigned long long umax = ~0ULL, smax = (1ULL << 63) - 1;
    unsigned long long number = 0;
    int neg = 1;

    *val_out = NULL;

    if (*ctx->p == '-') {
        neg = -1;
        ctx->p++;
    }

    if (!is_digit(*ctx->p))
        return EINVAL;

    /* Read the number into an unsigned 64-bit container, ensuring that we
     * don't overflow it. */
    while (is_digit(*ctx->p)) {
        if (number + 1 > umax / 10)
            return EOVERFLOW;
        number = (number * 10) + (*ctx->p - '0');
        ctx->p++;
    }

    /* Make sure the unsigned 64-bit value fits in the signed 64-bit range. */
    if (number > smax + 1 || (number > smax && neg == 1))
        return EOVERFLOW;

    return k5_json_number_create(number * neg, val_out);
}

/* Parse a JSON string (which must not quote Unicode code points above 256). */
static int
parse_string(struct decode_ctx *ctx, char **str_out)
{
    const unsigned char *p, *start, *end = NULL;
    const char *q;
    char *buf, *pos;
    unsigned int code;

    *str_out = NULL;

    /* Find the start and end of the string. */
    if (*ctx->p != '"')
        return EINVAL;
    start = ++ctx->p;
    for (; *ctx->p != '\0'; ctx->p++) {
        if (*ctx->p == '\\') {
            ctx->p++;
            if (*ctx->p == '\0')
                return EINVAL;
        } else if (*ctx->p == '"') {
            end = ctx->p++;
            break;
        }
    }
    if (end == NULL)
        return EINVAL;

    pos = buf = malloc(end - start + 1);
    if (buf == NULL)
        return ENOMEM;
    for (p = start; p < end;) {
        if (*p == '\\') {
            p++;
            if (*p == 'u' && is_hex_digit(p[1]) && is_hex_digit(p[2]) &&
                is_hex_digit(p[3]) && is_hex_digit(p[4])) {
                code = (hexval(p[1]) << 12) | (hexval(p[2]) << 8) |
                    (hexval(p[3]) << 4) | hexval(p[4]);
                if (code <= 0xff) {
                    *pos++ = code;
                } else {
                    /* Code points above 0xff don't need to be quoted, so we
                     * don't implement translating those into UTF-8. */
                    free(buf);
                    return EINVAL;
                }
                p += 5;
            } else {
                q = strchr(quotemap_json, *p);
                if (q != NULL) {
                    *pos++ = quotemap_c[q - quotemap_json];
                } else {
                    free(buf);
                    return EINVAL;
                }
                p++;
            }
        } else {
            *pos++ = *p++;
        }
    }
    *pos = '\0';
    *str_out = buf;
    return 0;
}

/* Parse an object association and place it into obj. */
static int
parse_object_association(k5_json_object obj, struct decode_ctx *ctx)
{
    char *key = NULL;
    k5_json_value val;
    int ret;

    /* Parse the key and value. */
    ret = parse_string(ctx, &key);
    if (ret)
        return ret;
    if (white_spaces(ctx))
        goto invalid;
    if (*ctx->p != ':')
        goto invalid;
    ctx->p++;
    if (white_spaces(ctx))
        goto invalid;
    ret = parse_value(ctx, &val);
    if (ret) {
        free(key);
        return ret;
    }

    /* Add the key and value to obj. */
    ret = k5_json_object_set(obj, key, val);
    free(key);
    k5_json_release(val);
    return ret;

invalid:
    free(key);
    return EINVAL;
}

/* Parse a JSON object. */
static int
parse_object(struct decode_ctx *ctx, k5_json_object *val_out)
{
    k5_json_object obj = NULL;
    int ret;

    *val_out = NULL;

    /* Parse past the opening brace. */
    if (*ctx->p != '{')
        return EINVAL;
    ctx->p++;
    if (white_spaces(ctx))
        return EINVAL;

    ret = k5_json_object_create(&obj);
    if (ret)
        return ret;

    /* Pairs associations until we reach the terminating brace. */
    if (*ctx->p != '}') {
        while (1) {
            ret = parse_object_association(obj, ctx);
            if (ret) {
                k5_json_release(obj);
                return ret;
            }
            if (white_spaces(ctx))
                goto invalid;
            if (*ctx->p == '}')
                break;
            if (*ctx->p != ',')
                goto invalid;
            ctx->p++;
            if (white_spaces(ctx))
                goto invalid;
        }
    }
    ctx->p++;
    *val_out = obj;
    return 0;

invalid:
    k5_json_release(obj);
    return EINVAL;
}

/* Parse an value and place it into array. */
static int
parse_array_item(k5_json_array array, struct decode_ctx *ctx)
{
    k5_json_value val;
    int ret;

    ret = parse_value(ctx, &val);
    if (ret)
        return ret;
    ret = k5_json_array_add(array, val);
    k5_json_release(val);
    return ret;
}

/* Parse a JSON array. */
static int
parse_array(struct decode_ctx *ctx, k5_json_array *val_out)
{
    k5_json_array array = NULL;
    int ret;

    *val_out = NULL;

    /* Parse past the opening bracket. */
    if (*ctx->p != '[')
        return EINVAL;
    ctx->p++;
    if (white_spaces(ctx))
        return EINVAL;

    ret = k5_json_array_create(&array);
    if (ret)
        return ret;

    /* Pairs values until we reach the terminating bracket. */
    if (*ctx->p != ']') {
        while (1) {
            ret = parse_array_item(array, ctx);
            if (ret) {
                k5_json_release(array);
                return ret;
            }
            if (white_spaces(ctx))
                goto invalid;
            if (*ctx->p == ']')
                break;
            if (*ctx->p != ',')
                goto invalid;
            ctx->p++;
            if (white_spaces(ctx))
                goto invalid;
        }
    }
    ctx->p++;
    *val_out = array;
    return 0;

invalid:
    k5_json_release(array);
    return EINVAL;
}

/* Parse a JSON value of any type. */
static int
parse_value(struct decode_ctx *ctx, k5_json_value *val_out)
{
    k5_json_null null;
    k5_json_bool bval;
    k5_json_number num;
    k5_json_string str;
    k5_json_object obj;
    k5_json_array array;
    char *cstring;
    int ret;

    *val_out = NULL;

    if (white_spaces(ctx))
        return EINVAL;

    if (*ctx->p == '"') {
        ret = parse_string(ctx, &cstring);
        if (ret)
            return ret;
        ret = k5_json_string_create(cstring, &str);
        free(cstring);
        if (ret)
            return ret;
        *val_out = str;
    } else if (*ctx->p == '{') {
        if (ctx->depth-- == 1)
            return EINVAL;
        ret = parse_object(ctx, &obj);
        if (ret)
            return ret;
        ctx->depth++;
        *val_out = obj;
    } else if (*ctx->p == '[') {
        if (ctx->depth-- == 1)
            return EINVAL;
        ret = parse_array(ctx, &array);
        ctx->depth++;
        *val_out = array;
    } else if (is_digit(*ctx->p) || *ctx->p == '-') {
        ret = parse_number(ctx, &num);
        if (ret)
            return ret;
        *val_out = num;
    } else if (strncmp((char *)ctx->p, "null", 4) == 0) {
        ctx->p += 4;
        ret = k5_json_null_create(&null);
        if (ret)
            return ret;
        *val_out = null;
    } else if (strncmp((char *)ctx->p, "true", 4) == 0) {
        ctx->p += 4;
        ret = k5_json_bool_create(1, &bval);
        if (ret)
            return ret;
        *val_out = bval;
    } else if (strncmp((char *)ctx->p, "false", 5) == 0) {
        ctx->p += 5;
        ret = k5_json_bool_create(0, &bval);
        if (ret)
            return ret;
        *val_out = bval;
    } else {
        return EINVAL;
    }

    return 0;
}

int
k5_json_decode(const char *string, k5_json_value *val_out)
{
    struct decode_ctx ctx;
    k5_json_value val;
    int ret;

    *val_out = NULL;
    ctx.p = (unsigned char *)string;
    ctx.depth = MAX_DECODE_DEPTH;
    ret = parse_value(&ctx, &val);
    if (ret)
        return ret;
    if (white_spaces(&ctx) == 0) {
        k5_json_release(val);
        return EINVAL;
    }
    *val_out = val;
    return 0;
}
