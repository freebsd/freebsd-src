/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/rcache/rc_file2.c - file-based replay cache, version 2 */
/*
 * Copyright (C) 2019 by the Massachusetts Institute of Technology.
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

#include "k5-int.h"
#include "k5-hashtab.h"
#include "rc-int.h"
#ifndef _WIN32
#include <sys/types.h>
#include <sys/stat.h>
#endif

#define MAX_SIZE INT32_MAX
#define TAG_LEN 12
#define RECORD_LEN (TAG_LEN + 4)
#define FIRST_TABLE_RECORDS 1023

/* Return the offset and number of records in the next table.  *offset should
 * initially be -1. */
static inline krb5_error_code
next_table(off_t *offset, off_t *nrecords)
{
    if (*offset == -1) {
        *offset = K5_HASH_SEED_LEN;
        *nrecords = FIRST_TABLE_RECORDS;
    } else if (*offset == K5_HASH_SEED_LEN) {
        *offset += *nrecords * RECORD_LEN;
        *nrecords = (FIRST_TABLE_RECORDS + 1) * 2;
    } else {
        *offset += *nrecords * RECORD_LEN;
        *nrecords *= 2;
    }

    /* Make sure the next table fits within the maximum file size. */
    if (*nrecords > MAX_SIZE / RECORD_LEN)
        return EOVERFLOW;
    if (*offset > MAX_SIZE - (*nrecords * RECORD_LEN))
        return EOVERFLOW;

    return 0;
}

/* Read up to two records from fd at offset, and parse them out into tags and
 * timestamps.  Place the number of records read in *nread. */
static krb5_error_code
read_records(int fd, off_t offset, uint8_t tag1_out[TAG_LEN],
             uint32_t *timestamp1_out, uint8_t tag2_out[TAG_LEN],
             uint32_t *timestamp2_out, int *nread)
{
    uint8_t buf[RECORD_LEN * 2];
    ssize_t st;

    *nread = 0;

    st = lseek(fd, offset, SEEK_SET);
    if (st == -1)
        return errno;
    st = read(fd, buf, RECORD_LEN * 2);
    if (st == -1)
        return errno;

    if (st >= RECORD_LEN) {
        memcpy(tag1_out, buf, TAG_LEN);
        *timestamp1_out = load_32_be(buf + TAG_LEN);
        *nread = 1;
    }
    if (st == RECORD_LEN * 2) {
        memcpy(tag2_out, buf + RECORD_LEN, TAG_LEN);
        *timestamp2_out = load_32_be(buf + RECORD_LEN + TAG_LEN);
        *nread = 2;
    }
    return 0;
}

/* Write one record to fd at offset, marshalling the tag and timestamp. */
static krb5_error_code
write_record(int fd, off_t offset, const uint8_t tag[TAG_LEN],
             uint32_t timestamp)
{
    uint8_t record[RECORD_LEN];
    ssize_t st;

    memcpy(record, tag, TAG_LEN);
    store_32_be(timestamp, record + TAG_LEN);

    st = lseek(fd, offset, SEEK_SET);
    if (st == -1)
        return errno;
    st = write(fd, record, RECORD_LEN);
    if (st == -1)
        return errno;
    if (st != RECORD_LEN) /* Unexpected for a regular file */
        return EIO;

    return 0;
}

/* Return true if timestamp is expired, for the current timestamp (now) and
 * allowable clock skew. */
static inline krb5_boolean
expired(uint32_t timestamp, uint32_t now, uint32_t skew)
{
    return ts_after(now, ts_incr(timestamp, skew));
}

/* Check and store a record into an open and locked file.  fd is assumed to be
 * at offset 0. */
static krb5_error_code
store(krb5_context context, int fd, const uint8_t tag[TAG_LEN], uint32_t now,
      uint32_t skew)
{
    krb5_error_code ret;
    krb5_data d;
    off_t table_offset = -1, nrecords = 0, avail_offset = -1, record_offset;
    ssize_t st;
    int ind, nread;
    uint8_t seed[K5_HASH_SEED_LEN], r1tag[TAG_LEN], r2tag[TAG_LEN];
    uint32_t r1stamp, r2stamp;

    /* Read or generate the hash seed. */
    st = read(fd, seed, sizeof(seed));
    if (st < 0)
        return errno;
    if ((size_t)st < sizeof(seed)) {
        d = make_data(seed, sizeof(seed));
        ret = krb5_c_random_make_octets(context, &d);
        if (ret)
            return ret;
        st = write(fd, seed, sizeof(seed));
        if (st < 0)
            return errno;
        if ((size_t)st != sizeof(seed))
            return EIO;
    }

    for (;;) {
        ret = next_table(&table_offset, &nrecords);
        if (ret)
            return ret;

        ind = k5_siphash24(tag, TAG_LEN, seed) % nrecords;
        record_offset = table_offset + ind * RECORD_LEN;

        ret = read_records(fd, record_offset, r1tag, &r1stamp, r2tag, &r2stamp,
                           &nread);
        if (ret)
            return ret;

        if ((nread >= 1 && r1stamp && memcmp(r1tag, tag, TAG_LEN) == 0) ||
            (nread == 2 && r2stamp && memcmp(r2tag, tag, TAG_LEN) == 0))
            return KRB5KRB_AP_ERR_REPEAT;

        /* Make note of the first record available for writing (empty, beyond
         * the end of the file, or expired). */
        if (avail_offset == -1) {
            if (nread == 0 || !r1stamp || expired(r1stamp, now, skew))
                avail_offset = record_offset;
            else if (nread == 1 || !r2stamp || expired(r2stamp, now, skew))
                avail_offset = record_offset + RECORD_LEN;
        }

        /* Stop searching if we encountered an empty record or one beyond the
         * end of the file, as tag would have been written there previously. */
        if (nread < 2 || !r1stamp || !r2stamp)
            return write_record(fd, avail_offset, tag, now);

        /* Use a different hash seed for the next table we search. */
        seed[0]++;
    }
}

krb5_error_code
k5_rcfile2_store(krb5_context context, int fd, const krb5_data *tag_data)
{
    krb5_error_code ret;
    krb5_timestamp now;
    uint8_t tagbuf[TAG_LEN], *tag;

    ret = krb5_timeofday(context, &now);
    if (ret)
        return ret;

    /* Extract a tag from the authenticator checksum. */
    if (tag_data->length >= TAG_LEN) {
        tag = (uint8_t *)tag_data->data;
    } else {
        memcpy(tagbuf, tag_data->data, tag_data->length);
        memset(tagbuf + tag_data->length, 0, TAG_LEN - tag_data->length);
        tag = tagbuf;
    }

    ret = krb5_lock_file(context, fd, KRB5_LOCKMODE_EXCLUSIVE);
    if (ret)
        return ret;
    ret = store(context, fd, tag, now, context->clockskew);
    (void)krb5_unlock_file(NULL, fd);
    return ret;
}

static krb5_error_code
file2_resolve(krb5_context context, const char *residual, void **rcdata_out)
{
    *rcdata_out = strdup(residual);
    return (*rcdata_out == NULL) ? ENOMEM : 0;
}

static void
file2_close(krb5_context context, void *rcdata)
{
    free(rcdata);
}

static krb5_error_code
file2_store(krb5_context context, void *rcdata, const krb5_data *tag)
{
    krb5_error_code ret;
    const char *filename = rcdata;
    int fd;

    fd = open(filename, O_CREAT | O_RDWR | O_BINARY, 0600);
    if (fd < 0) {
        ret = errno;
        k5_setmsg(context, ret, "%s (filename: %s)", error_message(ret),
                  filename);
        return ret;
    }
    ret = k5_rcfile2_store(context, fd, tag);
    close(fd);
    return ret;
}

const krb5_rc_ops k5_rc_file2_ops =
{
    "file2",
    file2_resolve,
    file2_close,
    file2_store
};
