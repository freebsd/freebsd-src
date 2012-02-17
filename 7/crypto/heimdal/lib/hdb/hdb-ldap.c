/*
 * Copyright (c) 1999-2001, PADL Software Pty Ltd.
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
 * 3. Neither the name of PADL Software  nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PADL SOFTWARE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL PADL SOFTWARE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "hdb_locl.h"

RCSID("$Id: hdb-ldap.c,v 1.10.4.1 2003/09/18 20:49:09 lha Exp $");

#ifdef OPENLDAP

#include <lber.h>
#include <ldap.h>
#include <ctype.h>
#include <sys/un.h>

static krb5_error_code LDAP__connect(krb5_context context, HDB * db);

static krb5_error_code
LDAP_message2entry(krb5_context context, HDB * db, LDAPMessage * msg,
		   hdb_entry * ent);

static char *krb5kdcentry_attrs[] =
    { "krb5PrincipalName", "cn", "krb5PrincipalRealm",
    "krb5KeyVersionNumber", "krb5Key",
    "krb5ValidStart", "krb5ValidEnd", "krb5PasswordEnd",
    "krb5MaxLife", "krb5MaxRenew", "krb5KDCFlags", "krb5EncryptionType",
    "modifiersName", "modifyTimestamp", "creatorsName", "createTimestamp",
    NULL
};

static char *krb5principal_attrs[] =
    { "krb5PrincipalName", "cn", "krb5PrincipalRealm",
    "modifiersName", "modifyTimestamp", "creatorsName", "createTimestamp",
    NULL
};

static krb5_error_code
LDAP__setmod(LDAPMod *** modlist, int modop, const char *attribute,
	int *pIndex)
{
    int cMods;

    if (*modlist == NULL) {
	*modlist = (LDAPMod **)ber_memcalloc(1, sizeof(LDAPMod *));
	if (*modlist == NULL) {
	    return ENOMEM;
	}
    }

    for (cMods = 0; (*modlist)[cMods] != NULL; cMods++) {
	if ((*modlist)[cMods]->mod_op == modop &&
	    strcasecmp((*modlist)[cMods]->mod_type, attribute) == 0) {
	    break;
	}
    }

    *pIndex = cMods;

    if ((*modlist)[cMods] == NULL) {
	LDAPMod *mod;

	*modlist = (LDAPMod **)ber_memrealloc(*modlist,
					      (cMods + 2) * sizeof(LDAPMod *));
	if (*modlist == NULL) {
	    return ENOMEM;
	}
	(*modlist)[cMods] = (LDAPMod *)ber_memalloc(sizeof(LDAPMod));
	if ((*modlist)[cMods] == NULL) {
	    return ENOMEM;
	}

	mod = (*modlist)[cMods];
	mod->mod_op = modop;
	mod->mod_type = ber_strdup(attribute);
	if (mod->mod_type == NULL) {
	    ber_memfree(mod);
	    (*modlist)[cMods] = NULL;
	    return ENOMEM;
	}

	if (modop & LDAP_MOD_BVALUES) {
	    mod->mod_bvalues = NULL;
	} else {
	    mod->mod_values = NULL;
	}

	(*modlist)[cMods + 1] = NULL;
    }

    return 0;
}

static krb5_error_code
LDAP_addmod_len(LDAPMod *** modlist, int modop, const char *attribute,
		unsigned char *value, size_t len)
{
    int cMods, cValues = 0;
    krb5_error_code ret;

    ret = LDAP__setmod(modlist, modop | LDAP_MOD_BVALUES, attribute, &cMods);
    if (ret != 0) {
	return ret;
    }

    if (value != NULL) {
	struct berval *bValue;
	struct berval ***pbValues = &((*modlist)[cMods]->mod_bvalues);

	if (*pbValues != NULL) {
	    for (cValues = 0; (*pbValues)[cValues] != NULL; cValues++)
		;
	    *pbValues = (struct berval **)ber_memrealloc(*pbValues, (cValues + 2)
							 * sizeof(struct berval *));
	} else {
	    *pbValues = (struct berval **)ber_memalloc(2 * sizeof(struct berval *));
	}
	if (*pbValues == NULL) {
	    return ENOMEM;
	}
	(*pbValues)[cValues] = (struct berval *)ber_memalloc(sizeof(struct berval));;
	if ((*pbValues)[cValues] == NULL) {
	    return ENOMEM;
	}

	bValue = (*pbValues)[cValues];
	bValue->bv_val = value;
	bValue->bv_len = len;

	(*pbValues)[cValues + 1] = NULL;
    }

    return 0;
}

static krb5_error_code
LDAP_addmod(LDAPMod *** modlist, int modop, const char *attribute,
	    const char *value)
{
    int cMods, cValues = 0;
    krb5_error_code ret;

    ret = LDAP__setmod(modlist, modop, attribute, &cMods);
    if (ret != 0) {
	return ret;
    }

    if (value != NULL) {
	char ***pValues = &((*modlist)[cMods]->mod_values);

	if (*pValues != NULL) {
	    for (cValues = 0; (*pValues)[cValues] != NULL; cValues++)
		;
	    *pValues = (char **)ber_memrealloc(*pValues, (cValues + 2) * sizeof(char *));
	} else {
	    *pValues = (char **)ber_memalloc(2 * sizeof(char *));
	}
	if (*pValues == NULL) {
	    return ENOMEM;
	}
	(*pValues)[cValues] = ber_strdup(value);
	if ((*pValues)[cValues] == NULL) {
	    return ENOMEM;
	}
	(*pValues)[cValues + 1] = NULL;
    }

    return 0;
}

static krb5_error_code
LDAP_addmod_generalized_time(LDAPMod *** mods, int modop,
			     const char *attribute, KerberosTime * time)
{
    char buf[22];
    struct tm *tm;

    /* XXX not threadsafe */
    tm = gmtime(time);
    strftime(buf, sizeof(buf), "%Y%m%d%H%M%SZ", tm);

    return LDAP_addmod(mods, modop, attribute, buf);
}

static krb5_error_code
LDAP_get_string_value(HDB * db, LDAPMessage * entry,
		      const char *attribute, char **ptr)
{
    char **vals;
    int ret;

    vals = ldap_get_values((LDAP *) db->db, entry, (char *) attribute);
    if (vals == NULL) {
	return HDB_ERR_NOENTRY;
    }
    *ptr = strdup(vals[0]);
    if (*ptr == NULL) {
	ret = ENOMEM;
    } else {
	ret = 0;
    }

    ldap_value_free(vals);

    return ret;
}

static krb5_error_code
LDAP_get_integer_value(HDB * db, LDAPMessage * entry,
		       const char *attribute, int *ptr)
{
    char **vals;

    vals = ldap_get_values((LDAP *) db->db, entry, (char *) attribute);
    if (vals == NULL) {
	return HDB_ERR_NOENTRY;
    }
    *ptr = atoi(vals[0]);
    ldap_value_free(vals);
    return 0;
}

static krb5_error_code
LDAP_get_generalized_time_value(HDB * db, LDAPMessage * entry,
				const char *attribute, KerberosTime * kt)
{
    char *tmp, *gentime;
    struct tm tm;
    int ret;

    *kt = 0;

    ret = LDAP_get_string_value(db, entry, attribute, &gentime);
    if (ret != 0) {
	return ret;
    }

    tmp = strptime(gentime, "%Y%m%d%H%M%SZ", &tm);
    if (tmp == NULL) {
	free(gentime);
	return HDB_ERR_NOENTRY;
    }

    free(gentime);

    *kt = timegm(&tm);

    return 0;
}

static krb5_error_code
LDAP_entry2mods(krb5_context context, HDB * db, hdb_entry * ent,
		LDAPMessage * msg, LDAPMod *** pmods)
{
    krb5_error_code ret;
    krb5_boolean is_new_entry;
    int rc, i;
    char *tmp = NULL;
    LDAPMod **mods = NULL;
    hdb_entry orig;
    unsigned long oflags, nflags;

    if (msg != NULL) {
	ret = LDAP_message2entry(context, db, msg, &orig);
	if (ret != 0) {
	    goto out;
	}
	is_new_entry = FALSE;
    } else {
	/* to make it perfectly obvious we're depending on
	 * orig being intiialized to zero */
	memset(&orig, 0, sizeof(orig));
	is_new_entry = TRUE;
    }

    if (is_new_entry) {
	ret = LDAP_addmod(&mods, LDAP_MOD_ADD, "objectClass", "top");
	if (ret != 0) {
	    goto out;
	}
	/* person is the structural object class */
	ret = LDAP_addmod(&mods, LDAP_MOD_ADD, "objectClass", "person");
	if (ret != 0) {
	    goto out;
	}
	ret =
	    LDAP_addmod(&mods, LDAP_MOD_ADD, "objectClass",
			"krb5Principal");
	if (ret != 0) {
	    goto out;
	}
	ret = LDAP_addmod(&mods, LDAP_MOD_ADD, "objectClass",
			  "krb5KDCEntry");
	if (ret != 0) {
	    goto out;
	}
    }

    if (is_new_entry ||
	krb5_principal_compare(context, ent->principal, orig.principal) ==
	FALSE) {
	ret = krb5_unparse_name(context, ent->principal, &tmp);
	if (ret != 0) {
	    goto out;
	}
	ret =
	    LDAP_addmod(&mods, LDAP_MOD_REPLACE, "krb5PrincipalName", tmp);
	if (ret != 0) {
	    free(tmp);
	    goto out;
	}
	free(tmp);
    }

    if (ent->kvno != orig.kvno) {
	rc = asprintf(&tmp, "%d", ent->kvno);
	if (rc < 0) {
	    krb5_set_error_string(context, "asprintf: out of memory");
	    ret = ENOMEM;
	    goto out;
	}
	ret =
	    LDAP_addmod(&mods, LDAP_MOD_REPLACE, "krb5KeyVersionNumber",
			tmp);
	free(tmp);
	if (ret != 0) {
	    goto out;
	}
    }

    if (ent->valid_start) {
	if (orig.valid_end == NULL
	    || (*(ent->valid_start) != *(orig.valid_start))) {
	    ret =
		LDAP_addmod_generalized_time(&mods, LDAP_MOD_REPLACE,
					     "krb5ValidStart",
					     ent->valid_start);
	    if (ret != 0) {
		goto out;
	    }
	}
    }

    if (ent->valid_end) {
	if (orig.valid_end == NULL
	    || (*(ent->valid_end) != *(orig.valid_end))) {
	    ret =
		LDAP_addmod_generalized_time(&mods, LDAP_MOD_REPLACE,
					     "krb5ValidEnd",
					     ent->valid_end);
	    if (ret != 0) {
		goto out;
	    }
	}
    }

    if (ent->pw_end) {
	if (orig.pw_end == NULL || (*(ent->pw_end) != *(orig.pw_end))) {
	    ret =
		LDAP_addmod_generalized_time(&mods, LDAP_MOD_REPLACE,
					     "krb5PasswordEnd",
					     ent->pw_end);
	    if (ret != 0) {
		goto out;
	    }
	}
    }

    if (ent->max_life) {
	if (orig.max_life == NULL
	    || (*(ent->max_life) != *(orig.max_life))) {
	    rc = asprintf(&tmp, "%d", *(ent->max_life));
	    if (rc < 0) {
		krb5_set_error_string(context, "asprintf: out of memory");
		ret = ENOMEM;
		goto out;
	    }
	    ret = LDAP_addmod(&mods, LDAP_MOD_REPLACE, "krb5MaxLife", tmp);
	    free(tmp);
	    if (ret != 0) {
		goto out;
	    }
	}
    }

    if (ent->max_renew) {
	if (orig.max_renew == NULL
	    || (*(ent->max_renew) != *(orig.max_renew))) {
	    rc = asprintf(&tmp, "%d", *(ent->max_renew));
	    if (rc < 0) {
		krb5_set_error_string(context, "asprintf: out of memory");
		ret = ENOMEM;
		goto out;
	    }
	    ret =
		LDAP_addmod(&mods, LDAP_MOD_REPLACE, "krb5MaxRenew", tmp);
	    free(tmp);
	    if (ret != 0) {
		goto out;
	    }
	}
    }

    oflags = HDBFlags2int(orig.flags);
    nflags = HDBFlags2int(ent->flags);

    if (oflags != nflags) {
	rc = asprintf(&tmp, "%lu", nflags);
	if (rc < 0) {
	    krb5_set_error_string(context, "asprintf: out of memory");
	    ret = ENOMEM;
	    goto out;
	}
	ret = LDAP_addmod(&mods, LDAP_MOD_REPLACE, "krb5KDCFlags", tmp);
	free(tmp);
	if (ret != 0) {
	    goto out;
	}
    }

    if (is_new_entry == FALSE && orig.keys.len > 0) {
	/* for the moment, clobber and replace keys. */
	ret = LDAP_addmod(&mods, LDAP_MOD_DELETE, "krb5Key", NULL);
	if (ret != 0) {
	    goto out;
	}
    }

    for (i = 0; i < ent->keys.len; i++) {
	unsigned char *buf;
	size_t len;

	ASN1_MALLOC_ENCODE(Key, buf, len, &ent->keys.val[i], &len, ret);
	if (ret != 0)
	    goto out;

	/* addmod_len _owns_ the key, doesn't need to copy it */
	ret = LDAP_addmod_len(&mods, LDAP_MOD_ADD, "krb5Key", buf, len);
	if (ret != 0) {
	    goto out;
	}
    }

    if (ent->etypes) {
	/* clobber and replace encryption types. */
	if (is_new_entry == FALSE) {
	    ret =
		LDAP_addmod(&mods, LDAP_MOD_DELETE, "krb5EncryptionType",
			    NULL);
	}
	for (i = 0; i < ent->etypes->len; i++) {
	    rc = asprintf(&tmp, "%d", ent->etypes->val[i]);
	    if (rc < 0) {
		krb5_set_error_string(context, "asprintf: out of memory");
		ret = ENOMEM;
		goto out;
	    }
	    free(tmp);
	    ret =
		LDAP_addmod(&mods, LDAP_MOD_ADD, "krb5EncryptionType",
			    tmp);
	    if (ret != 0) {
		goto out;
	    }
	}
    }

    /* for clarity */
    ret = 0;

  out:

    if (ret == 0) {
	*pmods = mods;
    } else if (mods != NULL) {
	ldap_mods_free(mods, 1);
	*pmods = NULL;
    }

    if (msg != NULL) {
	hdb_free_entry(context, &orig);
    }

    return ret;
}

static krb5_error_code
LDAP_dn2principal(krb5_context context, HDB * db, const char *dn,
		  krb5_principal * principal)
{
    krb5_error_code ret;
    int rc, limit = 1;
    char **values;
    LDAPMessage *res = NULL, *e;

    rc = ldap_set_option((LDAP *) db->db, LDAP_OPT_SIZELIMIT, (const void *)&limit);
    if (rc != LDAP_SUCCESS) {
	krb5_set_error_string(context, "ldap_set_option: %s", ldap_err2string(rc));
	ret = HDB_ERR_BADVERSION;
	goto out;
     }

    rc = ldap_search_s((LDAP *) db->db, dn, LDAP_SCOPE_BASE,
		       "(objectclass=krb5Principal)", krb5principal_attrs,
		       0, &res);
    if (rc != LDAP_SUCCESS) {
	krb5_set_error_string(context, "ldap_search_s: %s", ldap_err2string(rc));
	ret = HDB_ERR_NOENTRY;
	goto out;
    }

    e = ldap_first_entry((LDAP *) db->db, res);
    if (e == NULL) {
	ret = HDB_ERR_NOENTRY;
	goto out;
    }

    values = ldap_get_values((LDAP *) db->db, e, "krb5PrincipalName");
    if (values == NULL) {
	ret = HDB_ERR_NOENTRY;
	goto out;
    }

    ret = krb5_parse_name(context, values[0], principal);
    ldap_value_free(values);

  out:
    if (res != NULL) {
	ldap_msgfree(res);
    }
    return ret;
}

static krb5_error_code
LDAP__lookup_princ(krb5_context context, HDB * db, const char *princname,
		   LDAPMessage ** msg)
{
    krb5_error_code ret;
    int rc, limit = 1;
    char *filter = NULL;

    (void) LDAP__connect(context, db);

    rc =
	asprintf(&filter,
		 "(&(objectclass=krb5KDCEntry)(krb5PrincipalName=%s))",
		 princname);
    if (rc < 0) {
	krb5_set_error_string(context, "asprintf: out of memory");
	ret = ENOMEM;
	goto out;
    }

    rc = ldap_set_option((LDAP *) db->db, LDAP_OPT_SIZELIMIT, (const void *)&limit);
    if (rc != LDAP_SUCCESS) {
	krb5_set_error_string(context, "ldap_set_option: %s", ldap_err2string(rc));
	ret = HDB_ERR_BADVERSION;
	goto out;
    }

    rc = ldap_search_s((LDAP *) db->db, db->name, LDAP_SCOPE_ONELEVEL, filter, 
		       krb5kdcentry_attrs, 0, msg);
    if (rc != LDAP_SUCCESS) {
	krb5_set_error_string(context, "ldap_search_s: %s", ldap_err2string(rc));
	ret = HDB_ERR_NOENTRY;
	goto out;
    }

    ret = 0;

  out:
    if (filter != NULL) {
	free(filter);
    }
    return ret;
}

static krb5_error_code
LDAP_principal2message(krb5_context context, HDB * db,
		       krb5_principal princ, LDAPMessage ** msg)
{
    char *princname = NULL;
    krb5_error_code ret;

    ret = krb5_unparse_name(context, princ, &princname);
    if (ret != 0) {
	return ret;
    }

    ret = LDAP__lookup_princ(context, db, princname, msg);
    free(princname);

    return ret;
}

/*
 * Construct an hdb_entry from a directory entry.
 */
static krb5_error_code
LDAP_message2entry(krb5_context context, HDB * db, LDAPMessage * msg,
		   hdb_entry * ent)
{
    char *unparsed_name = NULL, *dn = NULL;
    int ret;
    unsigned long tmp;
    struct berval **keys;
    char **values;

    memset(ent, 0, sizeof(*ent));
    ent->flags = int2HDBFlags(0);

    ret =
	LDAP_get_string_value(db, msg, "krb5PrincipalName",
			      &unparsed_name);
    if (ret != 0) {
	return ret;
    }

    ret = krb5_parse_name(context, unparsed_name, &ent->principal);
    if (ret != 0) {
	goto out;
    }

    ret =
	LDAP_get_integer_value(db, msg, "krb5KeyVersionNumber",
			       &ent->kvno);
    if (ret != 0) {
	ent->kvno = 0;
    }

    keys = ldap_get_values_len((LDAP *) db->db, msg, "krb5Key");
    if (keys != NULL) {
	int i;
	size_t l;

	ent->keys.len = ldap_count_values_len(keys);
	ent->keys.val = (Key *) calloc(ent->keys.len, sizeof(Key));
	if (ent->keys.val == NULL) {
	    krb5_set_error_string(context, "calloc: out of memory");
	    ret = ENOMEM;
	    goto out;
	}
	for (i = 0; i < ent->keys.len; i++) {
	    decode_Key((unsigned char *) keys[i]->bv_val,
		       (size_t) keys[i]->bv_len, &ent->keys.val[i], &l);
	}
	ber_bvecfree(keys);
    } else {
#if 1
	/*
	 * This violates the ASN1 but it allows a principal to
	 * be related to a general directory entry without creating
	 * the keys. Hopefully it's OK.
	 */
	ent->keys.len = 0;
	ent->keys.val = NULL;
#else
	ret = HDB_ERR_NOENTRY;
	goto out;
#endif
    }

    ret =
	LDAP_get_generalized_time_value(db, msg, "createTimestamp",
					&ent->created_by.time);
    if (ret != 0) {
	ent->created_by.time = time(NULL);
    }

    ent->created_by.principal = NULL;

    ret = LDAP_get_string_value(db, msg, "creatorsName", &dn);
    if (ret == 0) {
	if (LDAP_dn2principal(context, db, dn, &ent->created_by.principal)
	    != 0) {
	    ent->created_by.principal = NULL;
	}
	free(dn);
    }

    ent->modified_by = (Event *) malloc(sizeof(Event));
    if (ent->modified_by == NULL) {
	krb5_set_error_string(context, "malloc: out of memory");
	ret = ENOMEM;
	goto out;
    }
    ret =
	LDAP_get_generalized_time_value(db, msg, "modifyTimestamp",
					&ent->modified_by->time);
    if (ret == 0) {
	ret = LDAP_get_string_value(db, msg, "modifiersName", &dn);
	if (LDAP_dn2principal
	    (context, db, dn, &ent->modified_by->principal) != 0) {
	    ent->modified_by->principal = NULL;
	}
	free(dn);
    } else {
	free(ent->modified_by);
	ent->modified_by = NULL;
    }

    if ((ent->valid_start = (KerberosTime *) malloc(sizeof(KerberosTime)))
	== NULL) {
	krb5_set_error_string(context, "malloc: out of memory");
	ret = ENOMEM;
	goto out;
    }
    ret =
	LDAP_get_generalized_time_value(db, msg, "krb5ValidStart",
					ent->valid_start);
    if (ret != 0) {
	/* OPTIONAL */
	free(ent->valid_start);
	ent->valid_start = NULL;
    }

    if ((ent->valid_end = (KerberosTime *) malloc(sizeof(KerberosTime))) ==
	NULL) {
	krb5_set_error_string(context, "malloc: out of memory");
	ret = ENOMEM;
	goto out;
    }
    ret =
	LDAP_get_generalized_time_value(db, msg, "krb5ValidEnd",
					ent->valid_end);
    if (ret != 0) {
	/* OPTIONAL */
	free(ent->valid_end);
	ent->valid_end = NULL;
    }

    if ((ent->pw_end = (KerberosTime *) malloc(sizeof(KerberosTime))) ==
	NULL) {
	krb5_set_error_string(context, "malloc: out of memory");
	ret = ENOMEM;
	goto out;
    }
    ret =
	LDAP_get_generalized_time_value(db, msg, "krb5PasswordEnd",
					ent->pw_end);
    if (ret != 0) {
	/* OPTIONAL */
	free(ent->pw_end);
	ent->pw_end = NULL;
    }

    ent->max_life = (int *) malloc(sizeof(int));
    if (ent->max_life == NULL) {
	krb5_set_error_string(context, "malloc: out of memory");
	ret = ENOMEM;
	goto out;
    }
    ret = LDAP_get_integer_value(db, msg, "krb5MaxLife", ent->max_life);
    if (ret != 0) {
	free(ent->max_life);
	ent->max_life = NULL;
    }

    ent->max_renew = (int *) malloc(sizeof(int));
    if (ent->max_renew == NULL) {
	krb5_set_error_string(context, "malloc: out of memory");
	ret = ENOMEM;
	goto out;
    }
    ret = LDAP_get_integer_value(db, msg, "krb5MaxRenew", ent->max_renew);
    if (ret != 0) {
	free(ent->max_renew);
	ent->max_renew = NULL;
    }

    values = ldap_get_values((LDAP *) db->db, msg, "krb5KDCFlags");
    if (values != NULL) {
	tmp = strtoul(values[0], (char **) NULL, 10);
	if (tmp == ULONG_MAX && errno == ERANGE) {
	    krb5_set_error_string(context, "strtoul: could not convert flag");
	    ret = ERANGE;
	    goto out;
	}
    } else {
	tmp = 0;
    }
    ent->flags = int2HDBFlags(tmp);

    values = ldap_get_values((LDAP *) db->db, msg, "krb5EncryptionType");
    if (values != NULL) {
	int i;

	ent->etypes = malloc(sizeof(*(ent->etypes)));
	if (ent->etypes == NULL) {
	    krb5_set_error_string(context, "malloc: out of memory");
	    ret = ENOMEM;
	    goto out;
	}
	ent->etypes->len = ldap_count_values(values);
	ent->etypes->val = calloc(ent->etypes->len, sizeof(int));
	for (i = 0; i < ent->etypes->len; i++) {
	    ent->etypes->val[i] = atoi(values[i]);
	}
	ldap_value_free(values);
    }

    ret = 0;

  out:
    if (unparsed_name != NULL) {
	free(unparsed_name);
    }

    if (ret != 0) {
	/* I don't think this frees ent itself. */
	hdb_free_entry(context, ent);
    }

    return ret;
}

static krb5_error_code LDAP_close(krb5_context context, HDB * db)
{
    ldap_unbind_ext((LDAP *) db->db, NULL, NULL);
    db->db = NULL;

    return 0;
}

static krb5_error_code
LDAP_lock(krb5_context context, HDB * db, int operation)
{
    return 0;
}

static krb5_error_code LDAP_unlock(krb5_context context, HDB * db)
{
    return 0;
}

static krb5_error_code
LDAP_seq(krb5_context context, HDB * db, unsigned flags, hdb_entry * entry)
{
    int msgid, rc, parserc;
    krb5_error_code ret;
    LDAPMessage *e;

    msgid = db->openp;		/* BOGUS OVERLOADING */
    if (msgid < 0) {
	return HDB_ERR_NOENTRY;
    }

    do {
	rc = ldap_result((LDAP *) db->db, msgid, LDAP_MSG_ONE, NULL, &e);
	switch (rc) {
	case LDAP_RES_SEARCH_ENTRY:
	    /* We have an entry. Parse it. */
	    ret = LDAP_message2entry(context, db, e, entry);
	    ldap_msgfree(e);
	    break;
	case LDAP_RES_SEARCH_RESULT:
	    /* We're probably at the end of the results. If not, abandon. */
	    parserc =
		ldap_parse_result((LDAP *) db->db, e, NULL, NULL, NULL,
				  NULL, NULL, 1);
	    if (parserc != LDAP_SUCCESS
		&& parserc != LDAP_MORE_RESULTS_TO_RETURN) {
	        krb5_set_error_string(context, "ldap_parse_result: %s", ldap_err2string(parserc));
		ldap_abandon((LDAP *) db->db, msgid);
	    }
	    ret = HDB_ERR_NOENTRY;
	    db->openp = -1;
	    break;
	case 0:
	case -1:
	default:
	    /* Some unspecified error (timeout?). Abandon. */
	    ldap_msgfree(e);
	    ldap_abandon((LDAP *) db->db, msgid);
	    ret = HDB_ERR_NOENTRY;
	    db->openp = -1;
	    break;
	}
    } while (rc == LDAP_RES_SEARCH_REFERENCE);

    if (ret == 0) {
	if (db->master_key_set && (flags & HDB_F_DECRYPT)) {
	    ret = hdb_unseal_keys(context, db, entry);
	    if (ret)
		hdb_free_entry(context,entry);
	}
    }

    return ret;
}

static krb5_error_code
LDAP_firstkey(krb5_context context, HDB * db, unsigned flags,
	      hdb_entry * entry)
{
    int msgid, limit = LDAP_NO_LIMIT, rc;

    (void) LDAP__connect(context, db);

    rc = ldap_set_option((LDAP *) db->db, LDAP_OPT_SIZELIMIT, (const void *)&limit);
    if (rc != LDAP_SUCCESS) {
	krb5_set_error_string(context, "ldap_set_option: %s", ldap_err2string(rc));
	return HDB_ERR_BADVERSION;
    }

    msgid = ldap_search((LDAP *) db->db, db->name,
			LDAP_SCOPE_ONELEVEL, "(objectclass=krb5KDCEntry)",
			krb5kdcentry_attrs, 0);
    if (msgid < 0) {
	return HDB_ERR_NOENTRY;
    }

    db->openp = msgid;

    return LDAP_seq(context, db, flags, entry);
}

static krb5_error_code
LDAP_nextkey(krb5_context context, HDB * db, unsigned flags,
	     hdb_entry * entry)
{
    return LDAP_seq(context, db, flags, entry);
}

static krb5_error_code
LDAP_rename(krb5_context context, HDB * db, const char *new_name)
{
    return HDB_ERR_DB_INUSE;
}

static krb5_error_code LDAP__connect(krb5_context context, HDB * db)
{
    int rc, version = LDAP_VERSION3;
    /*
     * Empty credentials to do a SASL bind with LDAP. Note that empty
     * different from NULL credentials. If you provide NULL
     * credentials instead of empty credentials you will get a SASL
     * bind in progress message.
     */
    struct berval bv = { 0, "" };

    if (db->db != NULL) {
	/* connection has been opened. ping server. */
	struct sockaddr_un addr;
	socklen_t len;
	int sd;

	if (ldap_get_option((LDAP *) db->db, LDAP_OPT_DESC, &sd) == 0 &&
	    getpeername(sd, (struct sockaddr *) &addr, &len) < 0) {
	    /* the other end has died. reopen. */
	    LDAP_close(context, db);
	}
    }

    if (db->db != NULL) {
	/* server is UP */
	return 0;
    }

    rc = ldap_initialize((LDAP **) & db->db, "ldapi:///");
    if (rc != LDAP_SUCCESS) {
	krb5_set_error_string(context, "ldap_initialize: %s", ldap_err2string(rc));
	return HDB_ERR_NOENTRY;
    }

    rc = ldap_set_option((LDAP *) db->db, LDAP_OPT_PROTOCOL_VERSION, (const void *)&version);
    if (rc != LDAP_SUCCESS) {
	krb5_set_error_string(context, "ldap_set_option: %s", ldap_err2string(rc));
	ldap_unbind_ext((LDAP *) db->db, NULL, NULL);
	db->db = NULL;
	return HDB_ERR_BADVERSION;
    }

    rc = ldap_sasl_bind_s((LDAP *) db->db, NULL, "EXTERNAL", &bv, NULL, NULL, NULL);
    if (rc != LDAP_SUCCESS) {
	krb5_set_error_string(context, "ldap_sasl_bind_s: %s", ldap_err2string(rc));
	ldap_unbind_ext((LDAP *) db->db, NULL, NULL);
	db->db = NULL;
	return HDB_ERR_BADVERSION;
    }

    return 0;
}

static krb5_error_code
LDAP_open(krb5_context context, HDB * db, int flags, mode_t mode)
{
    /* Not the right place for this. */
#ifdef HAVE_SIGACTION
    struct sigaction sa;

    sa.sa_flags = 0;
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);

    sigaction(SIGPIPE, &sa, NULL);
#else
    signal(SIGPIPE, SIG_IGN);
#endif /* HAVE_SIGACTION */

    return LDAP__connect(context, db);
}

static krb5_error_code
LDAP_fetch(krb5_context context, HDB * db, unsigned flags,
	   hdb_entry * entry)
{
    LDAPMessage *msg, *e;
    krb5_error_code ret;

    ret = LDAP_principal2message(context, db, entry->principal, &msg);
    if (ret != 0) {
	return ret;
    }

    e = ldap_first_entry((LDAP *) db->db, msg);
    if (e == NULL) {
	ret = HDB_ERR_NOENTRY;
	goto out;
    }

    ret = LDAP_message2entry(context, db, e, entry);
    if (ret == 0) {
	if (db->master_key_set && (flags & HDB_F_DECRYPT)) {
	    ret = hdb_unseal_keys(context, db, entry);
	    if (ret)
		hdb_free_entry(context,entry);
	}
    }

  out:
    ldap_msgfree(msg);

    return ret;
}

static krb5_error_code
LDAP_store(krb5_context context, HDB * db, unsigned flags,
	   hdb_entry * entry)
{
    LDAPMod **mods = NULL;
    krb5_error_code ret;
    const char *errfn;
    int rc;
    LDAPMessage *msg = NULL, *e = NULL;
    char *dn = NULL, *name = NULL;

    ret = krb5_unparse_name(context, entry->principal, &name);
    if (ret != 0) {
	goto out;
    }

    ret = LDAP__lookup_princ(context, db, name, &msg);
    if (ret == 0) {
	e = ldap_first_entry((LDAP *) db->db, msg);
    }

    ret = hdb_seal_keys(context, db, entry);
    if (ret != 0) {
	goto out;
    }

    /* turn new entry into LDAPMod array */
    ret = LDAP_entry2mods(context, db, entry, e, &mods);
    if (ret != 0) {
	goto out;
    }

    if (e == NULL) {
	/* Doesn't exist yet. */
	char *p;

	e = NULL;

	/* normalize the naming attribute */
	for (p = name; *p != '\0'; p++) {
	    *p = (char) tolower((int) *p);
	}

	/*
	 * We could do getpwnam() on the local component of
	 * the principal to find cn/sn but that's probably
	 * bad thing to do from inside a KDC. Better leave
	 * it to management tools.
	 */
	ret = LDAP_addmod(&mods, LDAP_MOD_ADD, "cn", name);
	if (ret < 0) {
	    goto out;
	}

	ret = LDAP_addmod(&mods, LDAP_MOD_ADD, "sn", name);
	if (ret < 0) {
	    goto out;
	}

	if (db->name != NULL) {
	    ret = asprintf(&dn, "cn=%s,%s", name, db->name);
	} else {
	    /* A bit bogus, but we don't have a search base */
	    ret = asprintf(&dn, "cn=%s", name);
	}
	if (ret < 0) {
	    krb5_set_error_string(context, "asprintf: out of memory");
	    ret = ENOMEM;
	    goto out;
	}
    } else if (flags & HDB_F_REPLACE) {
	/* Entry exists, and we're allowed to replace it. */
	dn = ldap_get_dn((LDAP *) db->db, e);
    } else {
	/* Entry exists, but we're not allowed to replace it. Bail. */
	ret = HDB_ERR_EXISTS;
	goto out;
    }

    /* write entry into directory */
    if (e == NULL) {
	/* didn't exist before */
	rc = ldap_add_s((LDAP *) db->db, dn, mods);
	errfn = "ldap_add_s";
    } else {
	/* already existed, send deltas only */
	rc = ldap_modify_s((LDAP *) db->db, dn, mods);
	errfn = "ldap_modify_s";
    }

    if (rc == LDAP_SUCCESS) {
	ret = 0;
    } else {
	krb5_set_error_string(context, "%s: %s (dn=%s) %s", 
			      errfn, name, dn, ldap_err2string(rc));
	ret = HDB_ERR_CANT_LOCK_DB;
    }

  out:
    /* free stuff */
    if (dn != NULL) {
	free(dn);
    }

    if (msg != NULL) {
	ldap_msgfree(msg);
    }

    if (mods != NULL) {
	ldap_mods_free(mods, 1);
    }

    if (name != NULL) {
	free(name);
    }

    return ret;
}

static krb5_error_code
LDAP_remove(krb5_context context, HDB * db, hdb_entry * entry)
{
    krb5_error_code ret;
    LDAPMessage *msg, *e;
    char *dn = NULL;
    int rc, limit = LDAP_NO_LIMIT;

    ret = LDAP_principal2message(context, db, entry->principal, &msg);
    if (ret != 0) {
	goto out;
    }

    e = ldap_first_entry((LDAP *) db->db, msg);
    if (e == NULL) {
	ret = HDB_ERR_NOENTRY;
	goto out;
    }

    dn = ldap_get_dn((LDAP *) db->db, e);
    if (dn == NULL) {
	ret = HDB_ERR_NOENTRY;
	goto out;
    }

    rc = ldap_set_option((LDAP *) db->db, LDAP_OPT_SIZELIMIT, (const void *)&limit);
    if (rc != LDAP_SUCCESS) {
	krb5_set_error_string(context, "ldap_set_option: %s", ldap_err2string(rc));
	ret = HDB_ERR_BADVERSION;
	goto out;
    }

    rc = ldap_delete_s((LDAP *) db->db, dn);
    if (rc == LDAP_SUCCESS) {
	ret = 0;
    } else {
	krb5_set_error_string(context, "ldap_delete_s: %s", ldap_err2string(rc));
	ret = HDB_ERR_CANT_LOCK_DB;
    }

  out:
    if (dn != NULL) {
	free(dn);
    }

    if (msg != NULL) {
	ldap_msgfree(msg);
    }

    return ret;
}

static krb5_error_code
LDAP__get(krb5_context context, HDB * db, krb5_data key, krb5_data * reply)
{
    fprintf(stderr, "LDAP__get not implemented\n");
    abort();
    return 0;
}

static krb5_error_code
LDAP__put(krb5_context context, HDB * db, int replace,
	  krb5_data key, krb5_data value)
{
    fprintf(stderr, "LDAP__put not implemented\n");
    abort();
    return 0;
}

static krb5_error_code
LDAP__del(krb5_context context, HDB * db, krb5_data key)
{
    fprintf(stderr, "LDAP__del not implemented\n");
    abort();
    return 0;
}

static krb5_error_code LDAP_destroy(krb5_context context, HDB * db)
{
    krb5_error_code ret;

    ret = hdb_clear_master_key(context, db);
    if (db->name != NULL) {
	free(db->name);
    }
    free(db);

    return ret;
}

krb5_error_code
hdb_ldap_create(krb5_context context, HDB ** db, const char *arg)
{
    *db = malloc(sizeof(**db));
    if (*db == NULL) {
	krb5_set_error_string(context, "malloc: out of memory");
	return ENOMEM;
    }

    (*db)->db = NULL;

    if (arg == NULL || arg[0] == '\0') {
	/*
	 * if no argument specified in the configuration file
	 * then use NULL, which tells OpenLDAP to look in
	 * the ldap.conf file. This doesn't work for
	 * writing entries because we don't know where to
	 * put new principals.
	 */
	(*db)->name = NULL;
    } else {
	(*db)->name = strdup(arg); 
	if ((*db)->name == NULL) {
	    krb5_set_error_string(context, "strdup: out of memory");
	    free(*db);
	    *db = NULL;
	    return ENOMEM;
	}
    }

    (*db)->master_key_set = 0;
    (*db)->openp = 0;
    (*db)->open = LDAP_open;
    (*db)->close = LDAP_close;
    (*db)->fetch = LDAP_fetch;
    (*db)->store = LDAP_store;
    (*db)->remove = LDAP_remove;
    (*db)->firstkey = LDAP_firstkey;
    (*db)->nextkey = LDAP_nextkey;
    (*db)->lock = LDAP_lock;
    (*db)->unlock = LDAP_unlock;
    (*db)->rename = LDAP_rename;
    /* can we ditch these? */
    (*db)->_get = LDAP__get;
    (*db)->_put = LDAP__put;
    (*db)->_del = LDAP__del;
    (*db)->destroy = LDAP_destroy;

    return 0;
}

#endif				/* OPENLDAP */
