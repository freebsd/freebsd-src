/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* kdc/tdumputil.c - utilities for tab-separated, etc. files */
/*
 * Copyright (C) 2015 by the Massachusetts Institute of Technology.
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
#include "k5-platform.h"        /* for vasprintf */
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tdumputil.h"

/*
 * Structure describing flavor of a tabular output format.
 *
 * fieldsep is the field separator
 *
 * recordsep is the record/line separator
 *
 * quotechar begins and ends a quoted field.  If an instance of quotechar
 * occurs within a quoted field value, it is doubled.
 *
 * Values are only quoted if they contain fieldsep, recordsep, or quotechar.
 */
struct flavor {
    int fieldsep;               /* field separator */
    int recordsep;              /* record separator */
    int quotechar;              /* quote character */
};

struct rechandle {
    FILE *fh;
    const char *rectype;
    int do_sep;
    struct flavor flavor;
};

static const struct flavor tabsep = {
    '\t',                       /* fieldsep */
    '\n',                       /* recordsep */
    '\0'                        /* quotechar */
};

static const struct flavor csv = {
    ',',                        /* fieldsep */
    '\n',                       /* recordsep */
    '"'                         /* quotechar */
};

/*
 * Double any quote characters present in a quoted field.
 */
static char *
qquote(struct flavor *fl, const char *s)
{
    const char *sp;
    struct k5buf buf;

    k5_buf_init_dynamic(&buf);
    for (sp = s; *sp != '\0'; sp++) {
        k5_buf_add_len(&buf, sp, 1);
        if (*sp == fl->quotechar)
            k5_buf_add_len(&buf, sp, 1);
    }
    return k5_buf_cstring(&buf);
}

/*
 * Write an optionally quoted field.
 */
static int
writequoted(struct rechandle *h, const char *fmt, va_list ap)
{
    int doquote = 0, ret;
    char *s = NULL, *qs = NULL;
    struct flavor fl = h->flavor;

    assert(fl.quotechar != '\0');
    ret = vasprintf(&s, fmt, ap);
    if (ret < 0)
        return ret;
    if (strchr(s, fl.fieldsep) != NULL)
        doquote = 1;
    if (strchr(s, fl.recordsep) != NULL)
        doquote = 1;
    if (strchr(s, fl.quotechar) != NULL)
        doquote = 1;

    if (doquote) {
        qs = qquote(&fl, s);
        if (qs == NULL) {
            ret = -1;
            goto cleanup;
        }
        ret = fprintf(h->fh, "%c%s%c", fl.quotechar, qs, fl.quotechar);
    } else {
        ret = fprintf(h->fh, "%s", s);
    }
cleanup:
    free(s);
    free(qs);
    return ret;
}

/*
 * Return a rechandle with the requested file handle and rectype.
 *
 * rectype must be a valid pointer for the entire lifetime of the rechandle (or
 * null)
 */
static struct rechandle *
rechandle_common(FILE *fh, const char *rectype)
{
    struct rechandle *h = calloc(1, sizeof(*h));

    if (h == NULL)
        return NULL;
    h->fh = fh;
    h->rectype = rectype;
    h->do_sep = 0;
    return h;
}

/*
 * Return a rechandle for tab-separated output.
 */
struct rechandle *
rechandle_tabsep(FILE *fh, const char *rectype)
{
    struct rechandle *h = rechandle_common(fh, rectype);

    if (h == NULL)
        return NULL;
    h->flavor = tabsep;
    return h;
}

/*
 * Return a rechandle for CSV output.
 */
struct rechandle *
rechandle_csv(FILE *fh, const char *rectype)
{
    struct rechandle *h = rechandle_common(fh, rectype);

    if (h == NULL)
        return NULL;
    h->flavor = csv;
    return h;
}

/*
 * Free a rechandle.
 */
void
rechandle_free(struct rechandle *h)
{
    free(h);
}

/*
 * Start a record.  This includes writing a record type prefix (rectype) if
 * specified.
 */
int
startrec(struct rechandle *h)
{
    if (h->rectype == NULL) {
        h->do_sep = 0;
        return 0;
    }
    h->do_sep = 1;
    return fputs(h->rectype, h->fh);
}

/*
 * Write a single field of a record.  This includes writing a separator
 * character, if appropriate.
 */
int
writefield(struct rechandle *h, const char *fmt, ...)
{
    int ret = 0;
    va_list ap;
    struct flavor fl = h->flavor;

    if (h->do_sep) {
        ret = fputc(fl.fieldsep, h->fh);
        if (ret < 0)
            return ret;
    }
    h->do_sep = 1;
    va_start(ap, fmt);
    if (fl.quotechar == '\0')
        ret = vfprintf(h->fh, fmt, ap);
    else
        ret = writequoted(h, fmt, ap);
    va_end(ap);
    return ret;
}

/*
 * Finish a record (line).
 */
int
endrec(struct rechandle *h)
{
    int ret = 0;
    struct flavor fl = h->flavor;

    ret = fputc(fl.recordsep, h->fh);
    h->do_sep = 0;
    return ret;
}

/*
 * Write a header line if h->rectype is null.  (If rectype is set, it will be
 * prefixed to output lines, most likely in a mixed record type output file, so
 * it doesn't make sense to output a header line in that case.)
 */
int
writeheader(struct rechandle *h, char * const *a)
{
    int ret = 0;
    char * const *p;

    if (h->rectype != NULL)
        return 0;
    for (p = a; *p != NULL; p++) {
        ret = writefield(h, "%s", *p);
        if (ret < 0)
            return ret;
    }
    ret = endrec(h);
    return ret;
}
