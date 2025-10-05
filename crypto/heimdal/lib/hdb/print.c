/*
 * Copyright (c) 1999-2005 Kungliga Tekniska Högskolan
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
 * 3. Neither the name of KTH nor the names of its contributors may be
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY KTH AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL KTH OR ITS CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. */

#include "hdb_locl.h"
#include <hex.h>
#include <ctype.h>

/*
   This is the present contents of a dump line. This might change at
   any time. Fields are separated by white space.

  principal
  keyblock
  	kvno
	keys...
		mkvno
		enctype
		keyvalue
		salt (- means use normal salt)
  creation date and principal
  modification date and principal
  principal valid from date (not used)
  principal valid end date (not used)
  principal key expires (not used)
  max ticket life
  max renewable life
  flags
  generation number
  */

/*
 * These utility functions return the number of bytes written or -1, and
 * they set an error in the context.
 */
static ssize_t
append_string(krb5_context context, krb5_storage *sp, const char *fmt, ...)
{
    ssize_t sz;
    char *s;
    int rc;
    va_list ap;
    va_start(ap, fmt);
    rc = vasprintf(&s, fmt, ap);
    va_end(ap);
    if(rc < 0) {
	krb5_set_error_message(context, ENOMEM, "malloc: out of memory");
	return -1;
    }
    sz = krb5_storage_write(sp, s, strlen(s));
    free(s);
    return sz;
}

static krb5_error_code
append_hex(krb5_context context, krb5_storage *sp,
           int always_encode, int lower, krb5_data *data)
{
    ssize_t sz;
    int printable = 1;
    size_t i;
    char *p;

    p = data->data;
    if (!always_encode) {
        for (i = 0; i < data->length; i++) {
            if (!isalnum((unsigned char)p[i]) && p[i] != '.'){
                printable = 0;
                break;
            }
        }
    }
    if (printable && !always_encode)
	return append_string(context, sp, "\"%.*s\"",
			     data->length, data->data);
    sz = hex_encode(data->data, data->length, &p);
    if (sz == -1) return sz;
    if (lower)
        strlwr(p);
    sz = append_string(context, sp, "%s", p);
    free(p);
    return sz;
}

static char *
time2str(time_t t)
{
    static char buf[128];
    strftime(buf, sizeof(buf), "%Y%m%d%H%M%S", gmtime(&t));
    return buf;
}

static ssize_t
append_event(krb5_context context, krb5_storage *sp, Event *ev)
{
    krb5_error_code ret;
    ssize_t sz;
    char *pr = NULL;
    if(ev == NULL)
	return append_string(context, sp, "- ");
    if (ev->principal != NULL) {
       ret = krb5_unparse_name(context, ev->principal, &pr);
       if (ret) return -1; /* krb5_unparse_name() sets error info */
    }
    sz = append_string(context, sp, "%s:%s ", time2str(ev->time),
                       pr ? pr : "UNKNOWN");
    free(pr);
    return sz;
}

#define KRB5_KDB_SALTTYPE_NORMAL        0
#define KRB5_KDB_SALTTYPE_V4            1
#define KRB5_KDB_SALTTYPE_NOREALM       2
#define KRB5_KDB_SALTTYPE_ONLYREALM     3
#define KRB5_KDB_SALTTYPE_SPECIAL       4
#define KRB5_KDB_SALTTYPE_AFS3          5

static ssize_t
append_mit_key(krb5_context context, krb5_storage *sp,
               krb5_const_principal princ,
               unsigned int kvno, Key *key)
{
    krb5_error_code ret;
    ssize_t sz;
    size_t key_versions = key->salt ? 2 : 1;
    size_t decrypted_key_length;
    char buf[2];
    krb5_data keylenbytes;
    unsigned int salttype;

    sz = append_string(context, sp, "\t%u\t%u\t%d\t%d\t", key_versions, kvno,
                        key->key.keytype, key->key.keyvalue.length + 2);
    if (sz == -1) return sz;
    ret = krb5_enctype_keysize(context, key->key.keytype, &decrypted_key_length);
    if (ret) return -1; /* XXX we lose the error code */
    buf[0] = decrypted_key_length & 0xff;
    buf[1] = (decrypted_key_length & 0xff00) >> 8;
    keylenbytes.data = buf;
    keylenbytes.length = sizeof (buf);
    sz = append_hex(context, sp, 1, 1, &keylenbytes);
    if (sz == -1) return sz;
    sz = append_hex(context, sp, 1, 1, &key->key.keyvalue);
    if (!key->salt)
        return sz;
    
    /* Map salt to MIT KDB style */
    if (key->salt->type == KRB5_PADATA_PW_SALT) {
        krb5_salt k5salt;

        /*
         * Compute normal salt and then see whether it matches the stored one
         */
        ret = krb5_get_pw_salt(context, princ, &k5salt);
        if (ret) return -1;
        if (k5salt.saltvalue.length == key->salt->salt.length &&
            memcmp(k5salt.saltvalue.data, key->salt->salt.data,
                   k5salt.saltvalue.length) == 0)
            salttype = KRB5_KDB_SALTTYPE_NORMAL; /* matches */
        else if (key->salt->salt.length == strlen(princ->realm) &&
                 memcmp(key->salt->salt.data, princ->realm,
                        key->salt->salt.length) == 0)
            salttype = KRB5_KDB_SALTTYPE_ONLYREALM; /* matches realm */
        else if (key->salt->salt.length == k5salt.saltvalue.length - strlen(princ->realm) &&
                 memcmp((char *)k5salt.saltvalue.data + strlen(princ->realm),
                        key->salt->salt.data, key->salt->salt.length) == 0)
            salttype = KRB5_KDB_SALTTYPE_NOREALM; /* matches w/o realm */
        else
            salttype = KRB5_KDB_SALTTYPE_NORMAL;  /* hope for best */

    } else if (key->salt->type == KRB5_PADATA_AFS3_SALT) {
        salttype = KRB5_KDB_SALTTYPE_AFS3;
    }
    sz = append_string(context, sp, "\t%u\t%u\t", salttype,
                       key->salt->salt.length);
    if (sz == -1) return sz;
    return append_hex(context, sp, 1, 1, &key->salt->salt);
}

static krb5_error_code
entry2string_int (krb5_context context, krb5_storage *sp, hdb_entry *ent)
{
    char *p;
    int i;
    krb5_error_code ret;

    /* --- principal */
    ret = krb5_unparse_name(context, ent->principal, &p);
    if(ret)
	return ret;
    append_string(context, sp, "%s ", p);
    free(p);
    /* --- kvno */
    append_string(context, sp, "%d", ent->kvno);
    /* --- keys */
    for(i = 0; i < ent->keys.len; i++){
	/* --- mkvno, keytype */
	if(ent->keys.val[i].mkvno)
	    append_string(context, sp, ":%d:%d:",
			  *ent->keys.val[i].mkvno,
			  ent->keys.val[i].key.keytype);
	else
	    append_string(context, sp, "::%d:",
			  ent->keys.val[i].key.keytype);
	/* --- keydata */
	append_hex(context, sp, 0, 0, &ent->keys.val[i].key.keyvalue);
	append_string(context, sp, ":");
	/* --- salt */
	if(ent->keys.val[i].salt){
	    append_string(context, sp, "%u/", ent->keys.val[i].salt->type);
	    append_hex(context, sp, 0, 0, &ent->keys.val[i].salt->salt);
	}else
	    append_string(context, sp, "-");
    }
    append_string(context, sp, " ");
    /* --- created by */
    append_event(context, sp, &ent->created_by);
    /* --- modified by */
    append_event(context, sp, ent->modified_by);

    /* --- valid start */
    if(ent->valid_start)
	append_string(context, sp, "%s ", time2str(*ent->valid_start));
    else
	append_string(context, sp, "- ");

    /* --- valid end */
    if(ent->valid_end)
	append_string(context, sp, "%s ", time2str(*ent->valid_end));
    else
	append_string(context, sp, "- ");

    /* --- password ends */
    if(ent->pw_end)
	append_string(context, sp, "%s ", time2str(*ent->pw_end));
    else
	append_string(context, sp, "- ");

    /* --- max life */
    if(ent->max_life)
	append_string(context, sp, "%d ", *ent->max_life);
    else
	append_string(context, sp, "- ");

    /* --- max renewable life */
    if(ent->max_renew)
	append_string(context, sp, "%d ", *ent->max_renew);
    else
	append_string(context, sp, "- ");

    /* --- flags */
    append_string(context, sp, "%d ", HDBFlags2int(ent->flags));

    /* --- generation number */
    if(ent->generation) {
	append_string(context, sp, "%s:%d:%d ", time2str(ent->generation->time),
		      ent->generation->usec,
		      ent->generation->gen);
    } else
	append_string(context, sp, "- ");

    /* --- extensions */
    if(ent->extensions && ent->extensions->len > 0) {
	for(i = 0; i < ent->extensions->len; i++) {
	    void *d;
	    size_t size, sz = 0;

	    ASN1_MALLOC_ENCODE(HDB_extension, d, size,
			       &ent->extensions->val[i], &sz, ret);
	    if (ret) {
		krb5_clear_error_message(context);
		return ret;
	    }
	    if(size != sz)
		krb5_abortx(context, "internal asn.1 encoder error");

	    if (hex_encode(d, size, &p) < 0) {
		free(d);
		krb5_set_error_message(context, ENOMEM, "malloc: out of memory");
		return ENOMEM;
	    }

	    free(d);
	    append_string(context, sp, "%s%s", p,
			  ent->extensions->len - 1 != i ? ":" : "");
	    free(p);
	}
    } else
	append_string(context, sp, "-");

    return 0;
}

#define KRB5_KDB_DISALLOW_POSTDATED     0x00000001
#define KRB5_KDB_DISALLOW_FORWARDABLE   0x00000002
#define KRB5_KDB_DISALLOW_TGT_BASED     0x00000004
#define KRB5_KDB_DISALLOW_RENEWABLE     0x00000008
#define KRB5_KDB_DISALLOW_PROXIABLE     0x00000010
#define KRB5_KDB_DISALLOW_DUP_SKEY      0x00000020
#define KRB5_KDB_DISALLOW_ALL_TIX       0x00000040
#define KRB5_KDB_REQUIRES_PRE_AUTH      0x00000080
#define KRB5_KDB_REQUIRES_HW_AUTH       0x00000100
#define KRB5_KDB_REQUIRES_PWCHANGE      0x00000200
#define KRB5_KDB_DISALLOW_SVR           0x00001000
#define KRB5_KDB_PWCHANGE_SERVICE       0x00002000
#define KRB5_KDB_SUPPORT_DESMD5         0x00004000
#define KRB5_KDB_NEW_PRINC              0x00008000

static int
flags_to_attr(HDBFlags flags)
{
    int a = 0;

    if (!flags.postdate)
        a |= KRB5_KDB_DISALLOW_POSTDATED;
    if (!flags.forwardable)
        a |= KRB5_KDB_DISALLOW_FORWARDABLE;
    if (flags.initial)
        a |= KRB5_KDB_DISALLOW_TGT_BASED;
    if (!flags.renewable)
        a |= KRB5_KDB_DISALLOW_RENEWABLE;
    if (!flags.proxiable)
        a |= KRB5_KDB_DISALLOW_PROXIABLE;
    if (flags.invalid)
        a |= KRB5_KDB_DISALLOW_ALL_TIX;
    if (flags.require_preauth)
        a |= KRB5_KDB_REQUIRES_PRE_AUTH;
    if (flags.require_hwauth)
        a |= KRB5_KDB_REQUIRES_HW_AUTH;
    if (!flags.server)
        a |= KRB5_KDB_DISALLOW_SVR;
    if (flags.change_pw)
        a |= KRB5_KDB_PWCHANGE_SERVICE;
    return a;
}

krb5_error_code
entry2mit_string_int(krb5_context context, krb5_storage *sp, hdb_entry *ent)
{
    krb5_error_code ret;
    ssize_t sz;
    size_t i, k;
    size_t num_tl_data = 0;
    size_t num_key_data = 0;
    char *p;
    HDB_Ext_KeySet *hist_keys = NULL;
    HDB_extension *extp;
    time_t last_pw_chg = 0;
    time_t exp = 0;
    time_t pwexp = 0;
    unsigned int max_life = 0;
    unsigned int max_renew = 0;

    /* Always create a modified_by entry. */
    num_tl_data++;

    ret = hdb_entry_get_pw_change_time(ent, &last_pw_chg);
    if (ret) return ret;
    if (last_pw_chg)
        num_tl_data++;

    extp = hdb_find_extension(ent, choice_HDB_extension_data_hist_keys);
    if (extp)
        hist_keys = &extp->data.u.hist_keys;

    for (i = 0; i < ent->keys.len;i++) {
	if (!mit_strong_etype(ent->keys.val[i].key.keytype))
            continue;
        num_key_data++;
    }
    if (hist_keys) {
        for (i = 0; i < hist_keys->len; i++) {
            /*
             * MIT uses the highest kvno as the current kvno instead of
             * tracking kvno separately, so we can't dump keysets with kvno
             * higher than the entry's kvno.
             */
            if (hist_keys->val[i].kvno >= ent->kvno)
                continue;
            for (k = 0; k < hist_keys->val[i].keys.len; k++) {
                if (ent->keys.val[k].key.keytype == ETYPE_DES_CBC_MD4 ||
                    ent->keys.val[k].key.keytype == ETYPE_DES_CBC_MD5)
                    continue;
                num_key_data++;
            }
        }
    }

    ret = krb5_unparse_name(context, ent->principal, &p);
    if (ret) return ret;
    sz = append_string(context, sp, "princ\t38\t%u\t%u\t%u\t0\t%s\t%d",
                       strlen(p), num_tl_data, num_key_data, p,
                       flags_to_attr(ent->flags));
    if (sz == -1) {
	free(p);
	return ENOMEM;
    }

    if (ent->max_life)
        max_life = *ent->max_life;
    if (ent->max_renew)
        max_renew = *ent->max_renew;
    if (ent->valid_end)
        exp = *ent->valid_end;
    if (ent->pw_end)
        pwexp = *ent->pw_end;

    sz = append_string(context, sp, "\t%u\t%u\t%u\t%u\t0\t0\t0",
                       max_life, max_renew, exp, pwexp);
    if (sz == -1) {
	free(p);
	return ENOMEM;
    }

    /* Dump TL data we know: last pw chg and modified_by */
#define mit_KRB5_TL_LAST_PWD_CHANGE     1
#define mit_KRB5_TL_MOD_PRINC           2
    if (last_pw_chg) {
        krb5_data d;
        time_t val;
        unsigned char *ptr;
        
        ptr = (unsigned char *)&last_pw_chg;
        val = ptr[0] | (ptr[1] << 8) | (ptr[2] << 16) | (ptr[3] << 24);
        d.data = &val;
        d.length = sizeof (last_pw_chg);
        sz = append_string(context, sp, "\t%u\t%u\t",
                           mit_KRB5_TL_LAST_PWD_CHANGE, d.length);
	if (sz == -1) {
	    free(p);
	    return ENOMEM;
	}
        sz = append_hex(context, sp, 1, 1, &d);
	if (sz == -1) {
	    free(p);
	    return ENOMEM;
	}
    }
    if (ent->modified_by) {
        krb5_data d;
        unsigned int val;
        size_t plen;
        unsigned char *ptr;
        char *modby_p;

	free(p);
        ptr = (unsigned char *)&ent->modified_by->time;
        val = ptr[0] | (ptr[1] << 8) | (ptr[2] << 16) | (ptr[3] << 24);
        d.data = &val;
        d.length = sizeof (ent->modified_by->time);
        ret = krb5_unparse_name(context, ent->modified_by->principal, &modby_p);
        if (ret) return ret;
        plen = strlen(modby_p);
        sz = append_string(context, sp, "\t%u\t%u\t",
                           mit_KRB5_TL_MOD_PRINC,
                           d.length + plen + 1 /* NULL counted */);
        if (sz == -1) {
            free(modby_p);
            return ENOMEM;
        }
        sz = append_hex(context, sp, 1, 1, &d);
        if (sz == -1) {
            free(modby_p);
            return ENOMEM;
        }
        d.data = modby_p;
        d.length = plen + 1;
        sz = append_hex(context, sp, 1, 1, &d);
        free(modby_p);
        if (sz == -1) return ENOMEM;
    } else {
        krb5_data d;
        unsigned int val;
        size_t plen;
        unsigned char *ptr;

	/* Fake the entry to make MIT happy. */
        ptr = (unsigned char *)&last_pw_chg;
        val = ptr[0] | (ptr[1] << 8) | (ptr[2] << 16) | (ptr[3] << 24);
        d.data = &val;
        d.length = sizeof (last_pw_chg);
        plen = strlen(p);
        sz = append_string(context, sp, "\t%u\t%u\t",
                           mit_KRB5_TL_MOD_PRINC,
                           d.length + plen + 1 /* NULL counted */);
	if (sz == -1) {
	    free(p);
	    return ENOMEM;
	}
        sz = append_hex(context, sp, 1, 1, &d);
	if (sz == -1) {
	    free(p);
	    return ENOMEM;
	}
        d.data = p;
        d.length = plen + 1;
        sz = append_hex(context, sp, 1, 1, &d);
	free(p);
        if (sz == -1) return ENOMEM;
    }
    /*
     * Dump keys (remembering to not include any with kvno higher than
     * the entry's because MIT doesn't track entry kvno separately from
     * the entry's keys -- max kvno is it)
     */
    for (i = 0; i < ent->keys.len; i++) {
	if (!mit_strong_etype(ent->keys.val[i].key.keytype))
            continue;
        sz = append_mit_key(context, sp, ent->principal, ent->kvno,
                            &ent->keys.val[i]);
        if (sz == -1) return ENOMEM;
    }
    for (i = 0; hist_keys && i < ent->kvno; i++) {
        size_t m;

        /* dump historical keys */
        for (k = 0; k < hist_keys->len; k++) {
            if (hist_keys->val[k].kvno != ent->kvno - i)
                continue;
            for (m = 0; m < hist_keys->val[k].keys.len; m++) {
                if (ent->keys.val[k].key.keytype == ETYPE_DES_CBC_MD4 ||
                    ent->keys.val[k].key.keytype == ETYPE_DES_CBC_MD5)
                    continue;
                sz = append_mit_key(context, sp, ent->principal,
                                    hist_keys->val[k].kvno,
                                    &hist_keys->val[k].keys.val[m]);
                if (sz == -1) return ENOMEM;
            }
        }
    }
    sz = append_string(context, sp, "\t-1;"); /* "extra data" */
    if (sz == -1) return ENOMEM;
    return 0;
}

krb5_error_code
hdb_entry2string(krb5_context context, hdb_entry *ent, char **str)
{
    krb5_error_code ret;
    krb5_data data;
    krb5_storage *sp;

    sp = krb5_storage_emem();
    if (sp == NULL) {
	krb5_set_error_message(context, ENOMEM, "malloc: out of memory");
	return ENOMEM;
    }

    ret = entry2string_int(context, sp, ent);
    if (ret) {
	krb5_storage_free(sp);
	return ret;
    }

    krb5_storage_write(sp, "\0", 1);
    krb5_storage_to_data(sp, &data);
    krb5_storage_free(sp);
    *str = data.data;
    return 0;
}

/* print a hdb_entry to (FILE*)data; suitable for hdb_foreach */

krb5_error_code
hdb_print_entry(krb5_context context, HDB *db, hdb_entry_ex *entry,
                void *data)
{
    struct hdb_print_entry_arg *parg = data;
    krb5_error_code ret;
    krb5_storage *sp;

    fflush(parg->out);
    sp = krb5_storage_from_fd(fileno(parg->out));
    if (sp == NULL) {
	krb5_set_error_message(context, ENOMEM, "malloc: out of memory");
	return ENOMEM;
    }

    switch (parg->fmt) {
    case HDB_DUMP_HEIMDAL:
        ret = entry2string_int(context, sp, &entry->entry);
        break;
    case HDB_DUMP_MIT:
        ret = entry2mit_string_int(context, sp, &entry->entry);
        break;
    default:
        heim_abort("Only two dump formats supported: Heimdal and MIT");
    }
    if (ret) {
	krb5_storage_free(sp);
	return ret;
    }

    krb5_storage_write(sp, "\n", 1);
    krb5_storage_free(sp);
    return 0;
}
