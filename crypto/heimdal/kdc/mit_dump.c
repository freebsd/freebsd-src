/*
 * Copyright (c) 2000 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
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

#include "hprop.h"

extern krb5_error_code _hdb_mdb_value2entry(krb5_context context,
                                            krb5_data *data,
                                            krb5_kvno target_kvno,
                                            hdb_entry *entry);

extern int _hdb_mit_dump2mitdb_entry(krb5_context context,
                                     char *line,
                                     krb5_storage *sp);



/*
can have any number of princ stanzas.
format is as follows (only \n indicates newlines)
princ\t%d\t (%d is KRB5_KDB_V1_BASE_LENGTH, always 38)
%d\t (strlen of principal e.g. shadow/foo@ANDREW.CMU.EDU)
%d\t (number of tl_data)
%d\t (number of key data, e.g. how many keys for this user)
%d\t (extra data length)
%s\t (principal name)
%d\t (attributes)
%d\t (max lifetime, seconds)
%d\t (max renewable life, seconds)
%d\t (expiration, seconds since epoch or 2145830400 for never)
%d\t (password expiration, seconds, 0 for never)
%d\t (last successful auth, seconds since epoch)
%d\t (last failed auth, per above)
%d\t (failed auth count)
foreach tl_data 0 to number of tl_data - 1 as above
  %d\t%d\t (data type, data length)
  foreach tl_data 0 to length-1
    %02x (tl data contents[element n])
  except if tl_data length is 0
    %d (always -1)
  \t
foreach key 0 to number of keys - 1 as above
  %d\t%d\t (key data version, kvno)
  foreach version 0 to key data version - 1 (a key or a salt)
    %d\t%d\t(data type for this key, data length for this key)
    foreach key data length 0 to length-1
      %02x (key data contents[element n])
    except if key_data length is 0
      %d (always -1)
    \t
foreach extra data length 0 to length - 1
  %02x (extra data part)
unless no extra data
  %d (always -1)
;\n

*/

static char *
nexttoken(char **p)
{
    char *q;
    do {
	q = strsep(p, " \t");
    } while(q && *q == '\0');
    return q;
}

#include <kadm5/admin.h>

static int
my_fgetln(FILE *f, char **buf, size_t *sz, size_t *len)
{
    char *p, *n;

    if (!*buf) {
        *buf = malloc(*sz ? *sz : 2048);
        if (!*buf)
            return ENOMEM;
        if (!*sz)
            *sz = 2048;
    }
    *len = 0;
    while ((p = fgets(&(*buf)[*len], *sz, f))) {
        if (strcspn(*buf, "\r\n") || feof(f)) {
            *len = strlen(*buf);
            return 0;
        }
        *len += strlen(&(*buf)[*len]); /* *len should be == *sz */
        n = realloc(buf, *sz + (*sz >> 1));
        if (!n) {
            free(*buf);
            *buf = NULL;
            *sz = 0;
            *len = 0;
            return ENOMEM;
        }
        *buf = n;
        *sz += *sz >> 1;
    }
    return 0; /* *len == 0 || no EOL -> EOF */
}

int
mit_prop_dump(void *arg, const char *file)
{
    krb5_error_code ret;
    size_t line_bufsz = 0;
    size_t line_len = 0;
    char *line = NULL;
    int lineno = 0;
    FILE *f;
    struct hdb_entry_ex ent;
    struct prop_data *pd = arg;
    krb5_storage *sp = NULL;
    krb5_data kdb_ent;

    memset(&ent, 0, sizeof (ent));
    f = fopen(file, "r");
    if (f == NULL)
	return errno;

    ret = ENOMEM;
    sp = krb5_storage_emem();
    if (!sp)
        goto out;
    while ((ret = my_fgetln(f, &line, &line_bufsz, &line_len)) == 0) {
        char *p = line;
        char *q;
        lineno++;

	if(strncmp(line, "kdb5_util", strlen("kdb5_util")) == 0) {
	    int major;
            q = nexttoken(&p);
            if (strcmp(q, "kdb5_util"))
                errx(1, "line %d: unknown version", lineno);
	    q = nexttoken(&p); /* load_dump */
	    if (strcmp(q, "load_dump"))
		errx(1, "line %d: unknown version", lineno);
	    q = nexttoken(&p); /* load_dump */
	    if (strcmp(q, "version"))
		errx(1, "line %d: unknown version", lineno);
	    q = nexttoken(&p); /* x.0 */
	    if (sscanf(q, "%d", &major) != 1)
		errx(1, "line %d: unknown version", lineno);
	    if (major != 4 && major != 5 && major != 6)
		errx(1, "unknown dump file format, got %d, expected 4-6",
		     major);
	    continue;
	} else if(strncmp(p, "policy", strlen("policy")) == 0) {
            warnx("line: %d: ignoring policy (not supported)", lineno);
	    continue;
	} else if(strncmp(p, "princ", strlen("princ")) != 0) {
	    warnx("line %d: not a principal", lineno);
	    continue;
	}
        krb5_storage_truncate(sp, 0);
        ret = _hdb_mit_dump2mitdb_entry(pd->context, line, sp);
        if (ret) break;
        ret = krb5_storage_to_data(sp, &kdb_ent);
        if (ret) break;
        ret = _hdb_mdb_value2entry(pd->context, &kdb_ent, 0, &ent.entry);
        krb5_data_free(&kdb_ent);
        if (ret) break;
	ret = v5_prop(pd->context, NULL, &ent, arg);
        hdb_free_entry(pd->context, &ent);
        if (ret) break;
    }

out:
    fclose(f);
    free(line);
    if (sp)
        krb5_storage_free(sp);
    if (ret && ret == ENOMEM)
        errx(1, "out of memory");
    if (ret)
        errx(1, "line %d: problem parsing dump line", lineno);
    return ret;
}

