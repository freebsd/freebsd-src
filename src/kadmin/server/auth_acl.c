/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* kadmin/server/auth_acl.c - ACL kadm5_auth module */
/*
 * Copyright 1995-2004, 2007, 2008, 2017 by the Massachusetts Institute of
 * Technology.  All Rights Reserved.
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

#include "k5-int.h"
#include <syslog.h>
#include <kadm5/admin.h>
#include <krb5/kadm5_auth_plugin.h>
#include "adm_proto.h"
#include <ctype.h>
#include "auth.h"

/*
 * Access control bits.
 */
#define ACL_ADD                 1
#define ACL_DELETE              2
#define ACL_MODIFY              4
#define ACL_CHANGEPW            8
/* #define ACL_CHANGE_OWN_PW    16 */
#define ACL_INQUIRE             32
#define ACL_EXTRACT             64
#define ACL_LIST                128
#define ACL_SETKEY              256
#define ACL_IPROP               512

#define ACL_ALL_MASK            (ACL_ADD        |       \
                                 ACL_DELETE     |       \
                                 ACL_MODIFY     |       \
                                 ACL_CHANGEPW   |       \
                                 ACL_INQUIRE    |       \
                                 ACL_LIST       |       \
                                 ACL_IPROP      |       \
                                 ACL_SETKEY)

struct acl_op_table {
    char op;
    uint32_t mask;
};

struct acl_entry {
    struct acl_entry *next;
    krb5_principal client;
    uint32_t op_allowed;
    krb5_principal target;
    struct kadm5_auth_restrictions *rs;
};

static const struct acl_op_table acl_op_table[] = {
    { 'a', ACL_ADD },
    { 'd', ACL_DELETE },
    { 'm', ACL_MODIFY },
    { 'c', ACL_CHANGEPW },
    { 'i', ACL_INQUIRE },
    { 'l', ACL_LIST },
    { 'p', ACL_IPROP },
    { 's', ACL_SETKEY },
    { 'x', ACL_ALL_MASK },
    { '*', ACL_ALL_MASK },
    { 'e', ACL_EXTRACT },
    { '\0', 0 }
};

struct wildstate {
    int nwild;
    const krb5_data *backref[9];
};

struct acl_state {
    struct acl_entry *list;
};

/*
 * Get a line from the ACL file.  Lines ending with \ are continued on the next
 * line.  The caller should set *lineno to 1 and *incr to 0 before the first
 * call.  On successful return, *lineno will be the line number of the line
 * read.  Return a pointer to the line on success, or NULL on end of file or
 * read failure.
 */
static char *
get_line(FILE *fp, const char *fname, int *lineno, int *incr)
{
    const int chunksize = 128;
    struct k5buf buf;
    size_t old_len;
    char *p;

    /* Increment *lineno by the number of newlines from the last line. */
    *lineno += *incr;
    *incr = 0;

    k5_buf_init_dynamic(&buf);
    for (;;) {
        /* Read at least part of a line into the buffer. */
        old_len = buf.len;
        p = k5_buf_get_space(&buf, chunksize);
        if (p == NULL)
            return NULL;

        if (fgets(p, chunksize, fp) == NULL) {
            /* We reached the end.  Return a final unterminated line, if there
             * is one and it's not a comment. */
            k5_buf_truncate(&buf, old_len);
            if (buf.len > 0 && *(char *)buf.data != '#')
                return buf.data;
            k5_buf_free(&buf);
            return NULL;
        }

        /* Set the buffer length based on the actual amount read. */
        k5_buf_truncate(&buf, old_len + strlen(p));

        p = buf.data;
        if (buf.len > 0 && p[buf.len - 1] == '\n') {
            /* We have a complete raw line in the buffer. */
            (*incr)++;
            k5_buf_truncate(&buf, buf.len - 1);
            if (buf.len > 0 && p[buf.len - 1] == '\\') {
                /* This line has a continuation marker; keep reading. */
                k5_buf_truncate(&buf, buf.len - 1);
            } else if (buf.len == 0 || *p == '#') {
                /* This line is empty or a comment.  Start over. */
                *lineno += *incr;
                *incr = 0;
                k5_buf_truncate(&buf, 0);
            } else {
                return buf.data;
            }
        }
    }
}

/*
 * Parse a restrictions field.  Return NULL on failure.
 *
 * Allowed restrictions are:
 *      [+-]flagname            (recognized by krb5_flagspec_to_mask)
 *                              flag is forced to indicated value
 *      -clearpolicy            policy is forced clear
 *      -policy pol             policy is forced to be "pol"
 *      -{expire,pwexpire,maxlife,maxrenewlife} deltat
 *                              associated value will be forced to
 *                              MIN(deltat, requested value)
 */
static struct kadm5_auth_restrictions *
parse_restrictions(const char *str, const char *fname)
{
    char *copy = NULL, *token, *arg, *save;
    const char *delims = "\t\n\f\v\r ,";
    krb5_deltat delta;
    struct kadm5_auth_restrictions *rs;

    copy = strdup(str);
    if (copy == NULL)
        return NULL;

    rs = calloc(1, sizeof(*rs));
    if (rs == NULL) {
        free(copy);
        return NULL;
    }

    rs->forbid_attrs = ~(krb5_flags)0;
    for (token = strtok_r(copy, delims, &save); token != NULL;
         token = strtok_r(NULL, delims, &save)) {

        if (krb5_flagspec_to_mask(token, &rs->require_attrs,
                                  &rs->forbid_attrs) == 0) {
            rs->mask |= KADM5_ATTRIBUTES;
            continue;
        }

        if (strcmp(token, "-clearpolicy") == 0) {
            rs->mask |= KADM5_POLICY_CLR;
            continue;
        }

        /* Everything else needs an argument. */
        arg = strtok_r(NULL, delims, &save);
        if (arg == NULL)
            goto error;

        if (strcmp(token, "-policy") == 0) {
            if (rs->policy != NULL)
                goto error;
            rs->policy = strdup(arg);
            if (rs->policy == NULL)
                goto error;
            rs->mask |= KADM5_POLICY;
            continue;
        }

        /* All other arguments must be a deltat. */
        if (krb5_string_to_deltat(arg, &delta) != 0)
            goto error;

        if (strcmp(token, "-expire") == 0) {
            rs->princ_lifetime = delta;
            rs->mask |= KADM5_PRINC_EXPIRE_TIME;
        } else if (strcmp(token, "-pwexpire") == 0) {
            rs->pw_lifetime = delta;
            rs->mask |= KADM5_PW_EXPIRATION;
        } else if (strcmp(token, "-maxlife") == 0) {
            rs->max_life = delta;
            rs->mask |= KADM5_MAX_LIFE;
        } else if (strcmp(token, "-maxrenewlife") == 0) {
            rs->max_renewable_life = delta;
            rs->mask |= KADM5_MAX_RLIFE;
        } else {
            goto error;
        }
    }

    free(copy);
    return rs;

error:
    krb5_klog_syslog(LOG_ERR, _("%s: invalid restrictions: %s"), fname, str);
    free(copy);
    free(rs->policy);
    free(rs);
    return NULL;
}

static void
free_acl_entry(struct acl_entry *entry)
{
    krb5_free_principal(NULL, entry->client);
    krb5_free_principal(NULL, entry->target);
    if (entry->rs != NULL) {
        free(entry->rs->policy);
        free(entry->rs);
    }
    free(entry);
}

/* Parse the four fields of an ACL entry and return a structure representing
 * it.  Log a message and return NULL on error. */
static struct acl_entry *
parse_entry(krb5_context context, const char *client, const char *ops,
            const char *target, const char *rs, const char *line,
            const char *fname)
{
    struct acl_entry *entry;
    const char *op;
    char rop;
    int t;

    entry = calloc(1, sizeof(*entry));
    if (entry == NULL)
        return NULL;

    for (op = ops; *op; op++) {
        rop = isupper((unsigned char)*op) ? tolower((unsigned char)*op) : *op;
        for (t = 0; acl_op_table[t].op; t++) {
            if (rop == acl_op_table[t].op) {
                if (rop == *op)
                    entry->op_allowed |= acl_op_table[t].mask;
                else
                    entry->op_allowed &= ~acl_op_table[t].mask;
                break;
            }
        }
        if (!acl_op_table[t].op) {
            krb5_klog_syslog(LOG_ERR,
                             _("Unrecognized ACL operation '%c' in %s"),
                             *op, line);
            goto error;
        }
    }

    if (strcmp(client, "*") != 0) {
        if (krb5_parse_name(context, client, &entry->client) != 0) {
            krb5_klog_syslog(LOG_ERR, _("Cannot parse client principal '%s'"),
                             client);
            goto error;
        }
    }

    if (target != NULL && strcmp(target, "*") != 0) {
        if (krb5_parse_name(context, target, &entry->target) != 0) {
            krb5_klog_syslog(LOG_ERR, _("Cannot parse target principal '%s'"),
                             target);
            goto error;
        }
    }

    if (rs != NULL) {
        entry->rs = parse_restrictions(rs, fname);
        if (entry->rs == NULL)
            goto error;
    }

    return entry;

error:
    free_acl_entry(entry);
    return NULL;
}

/* Parse the contents of an ACL line. */
static struct acl_entry *
parse_line(krb5_context context, const char *line, const char *fname)
{
    struct acl_entry *entry = NULL;
    char *copy;
    char *client, *client_end, *ops, *ops_end, *target, *target_end, *rs, *end;
    const char *ws = "\t\n\f\v\r ,";

    /*
     * Format:
     *  entry ::= [<whitespace>] <principal> <whitespace> <opstring>
     *            [<whitespace> <target> [<whitespace> <restrictions>
     *                                    [<whitespace>]]]
     */

    /* Make a copy and remove any trailing whitespace. */
    copy = strdup(line);
    if (copy == NULL)
        return NULL;
    end = copy + strlen(copy);
    while (end > copy && isspace(end[-1]))
        *--end = '\0';

    /* Find the beginning and end of each field.  The end of restrictions is
     * the end of copy. */
    client = copy + strspn(copy, ws);
    client_end = client + strcspn(client, ws);
    ops = client_end + strspn(client_end, ws);
    ops_end = ops + strcspn(ops, ws);
    target = ops_end + strspn(ops_end, ws);
    target_end = target + strcspn(target, ws);
    rs = target_end + strspn(target_end, ws);

    /* Terminate the first three fields. */
    *client_end = *ops_end = *target_end = '\0';

    /* The last two fields are optional; represent them as NULL if not present.
     * The first two fields are required. */
    if (*target == '\0')
        target = NULL;
    if (*rs == '\0')
        rs = NULL;
    if (*client != '\0' && *ops != '\0')
        entry = parse_entry(context, client, ops, target, rs, line, fname);
    free(copy);
    return entry;
}

/* Free all ACL entries. */
static void
free_acl_entries(struct acl_state *state)
{
    struct acl_entry *entry, *next;

    for (entry = state->list; entry != NULL; entry = next) {
        next = entry->next;
        free_acl_entry(entry);
    }
    state->list = NULL;
}

/* Open and parse the ACL file. */
static krb5_error_code
load_acl_file(krb5_context context, const char *fname, struct acl_state *state)
{
    krb5_error_code ret;
    FILE *fp;
    char *line;
    struct acl_entry **entry_slot;
    int lineno, incr;

    state->list = NULL;

    /* Open the ACL file for reading. */
    fp = fopen(fname, "r");
    if (fp == NULL) {
        krb5_klog_syslog(LOG_ERR, _("%s while opening ACL file %s"),
                         error_message(errno), fname);
        ret = errno;
        k5_setmsg(context, errno, _("Cannot open %s: %s"), fname,
                  error_message(ret));
        return ret;
    }

    set_cloexec_file(fp);
    lineno = 1;
    incr = 0;
    entry_slot = &state->list;

    /* Get a non-comment line. */
    while ((line = get_line(fp, fname, &lineno, &incr)) != NULL) {
        /* Parse it.  Fail out on syntax error. */
        *entry_slot = parse_line(context, line, fname);
        if (*entry_slot == NULL) {
            krb5_klog_syslog(LOG_ERR,
                             _("%s: syntax error at line %d <%.10s...>"),
                             fname, lineno, line);
            k5_setmsg(context, EINVAL,
                      _("%s: syntax error at line %d <%.10s...>"),
                      fname, lineno, line);
            free_acl_entries(state);
            free(line);
            fclose(fp);
            return EINVAL;
        }
        entry_slot = &(*entry_slot)->next;
        free(line);
    }

    fclose(fp);
    return 0;
}

/*
 * See if two data entries match.  If e1 is a wildcard (matching a whole
 * component only) and targetflag is false, save an alias to e2 into
 * ws->backref.  If e1 is a back-reference and targetflag is true, compare the
 * appropriate entry in ws->backref to e2.  If ws is NULL, do not store or
 * match back-references.
 */
static krb5_boolean
match_data(const krb5_data *e1, const krb5_data *e2, krb5_boolean targetflag,
           struct wildstate *ws)
{
    int n;

    if (data_eq_string(*e1, "*")) {
        if (ws != NULL && !targetflag) {
            if (ws->nwild < 9)
                ws->backref[ws->nwild++] = e2;
        }
        return TRUE;
    }

    if (ws != NULL && targetflag && e1->length == 2 && e1->data[0] == '*' &&
        e1->data[1] >= '1' && e1->data[1] <= '9') {
        n = e1->data[1] - '1';
        if (n >= ws->nwild)
            return FALSE;
        return data_eq(*e2, *ws->backref[n]);
    } else {
        return data_eq(*e2, *e1);
    }
}

/* Return true if p1 matches p2.  p1 may contain wildcards if targetflag is
 * false, or backreferences if it is true. */
static krb5_boolean
match_princ(krb5_const_principal p1, krb5_const_principal p2,
            krb5_boolean targetflag, struct wildstate *ws)
{
    int i;

    /* The principals must be of the same length. */
    if (p1->length != p2->length)
        return FALSE;

    /* The realm must match, and does not interact with wildcard state. */
    if (!match_data(&p1->realm, &p2->realm, targetflag, NULL))
        return FALSE;

    /* All components of the principals must match. */
    for (i = 0; i < p1->length; i++) {
        if (!match_data(&p1->data[i], &p2->data[i], targetflag, ws))
            return FALSE;
    }

    return TRUE;
}

/* Find an ACL entry matching principal and target_principal.  Return NULL if
 * none is found. */
static struct acl_entry *
find_entry(struct acl_state *state, krb5_const_principal client,
           krb5_const_principal target)
{
    struct acl_entry *entry;
    struct wildstate ws;

    for (entry = state->list; entry != NULL; entry = entry->next) {
        memset(&ws, 0, sizeof(ws));
        if (entry->client != NULL) {
            if (!match_princ(entry->client, client, FALSE, &ws))
                continue;
        }

        if (entry->target != NULL) {
            if (target == NULL)
                continue;
            if (!match_princ(entry->target, target, TRUE, &ws))
                continue;
        }

        return entry;
    }

    return NULL;
}

/* Return true if op is permitted for this principal.  Set *rs_out (if not
 * NULL) according to any restrictions in the ACL entry. */
static krb5_error_code
acl_check(kadm5_auth_moddata data, uint32_t op, krb5_const_principal client,
          krb5_const_principal target, struct kadm5_auth_restrictions **rs_out)
{
    struct acl_entry *entry;

    if (rs_out != NULL)
        *rs_out = NULL;

    entry = find_entry((struct acl_state *)data, client, target);
    if (entry == NULL)
        return KRB5_PLUGIN_NO_HANDLE;
    if (!(entry->op_allowed & op))
        return KRB5_PLUGIN_NO_HANDLE;

    if (rs_out != NULL && entry->rs != NULL && entry->rs->mask)
        *rs_out = entry->rs;

    return 0;
}

static krb5_error_code
acl_init(krb5_context context, const char *acl_file,
         kadm5_auth_moddata *data_out)
{
    krb5_error_code ret;
    struct acl_state *state;

    *data_out = NULL;
    if (acl_file == NULL)
        return KRB5_PLUGIN_NO_HANDLE;
    state = malloc(sizeof(*state));
    state->list = NULL;
    ret = load_acl_file(context, acl_file, state);
    if (ret) {
        free(state);
        return ret;
    }
    *data_out = (kadm5_auth_moddata)state;
    return 0;
}

static void
acl_fini(krb5_context context, kadm5_auth_moddata data)
{
    if (data == NULL)
        return;
    free_acl_entries((struct acl_state *)data);
    free(data);
}

static krb5_error_code
acl_addprinc(krb5_context context, kadm5_auth_moddata data,
             krb5_const_principal client, krb5_const_principal target,
             const struct _kadm5_principal_ent_t *ent, long mask,
             struct kadm5_auth_restrictions **rs_out)
{
    return acl_check(data, ACL_ADD, client, target, rs_out);
}

static krb5_error_code
acl_modprinc(krb5_context context, kadm5_auth_moddata data,
             krb5_const_principal client, krb5_const_principal target,
             const struct _kadm5_principal_ent_t *ent, long mask,
             struct kadm5_auth_restrictions **rs_out)
{
    return acl_check(data, ACL_MODIFY, client, target, rs_out);
}

static krb5_error_code
acl_setstr(krb5_context context, kadm5_auth_moddata data,
           krb5_const_principal client, krb5_const_principal target,
           const char *key, const char *value)
{
    return acl_check(data, ACL_MODIFY, client, target, NULL);
}

static krb5_error_code
acl_cpw(krb5_context context, kadm5_auth_moddata data,
        krb5_const_principal client, krb5_const_principal target)
{
    return acl_check(data, ACL_CHANGEPW, client, target, NULL);
}

static krb5_error_code
acl_chrand(krb5_context context, kadm5_auth_moddata data,
           krb5_const_principal client, krb5_const_principal target)
{
    return acl_check(data, ACL_CHANGEPW, client, target, NULL);
}

static krb5_error_code
acl_setkey(krb5_context context, kadm5_auth_moddata data,
           krb5_const_principal client, krb5_const_principal target)
{
    return acl_check(data, ACL_SETKEY, client, target, NULL);
}

static krb5_error_code
acl_purgekeys(krb5_context context, kadm5_auth_moddata data,
              krb5_const_principal client, krb5_const_principal target)
{
    return acl_check(data, ACL_MODIFY, client, target, NULL);
}

static krb5_error_code
acl_delprinc(krb5_context context, kadm5_auth_moddata data,
             krb5_const_principal client, krb5_const_principal target)
{
    return acl_check(data, ACL_DELETE, client, target, NULL);
}

static krb5_error_code
acl_renprinc(krb5_context context, kadm5_auth_moddata data,
             krb5_const_principal client, krb5_const_principal src,
             krb5_const_principal dest)
{
    struct kadm5_auth_restrictions *rs;

    if (acl_check(data, ACL_DELETE, client, src, NULL) == 0 &&
        acl_check(data, ACL_ADD, client, dest, &rs) == 0 && rs == NULL)
        return 0;
    return KRB5_PLUGIN_NO_HANDLE;
}

static krb5_error_code
acl_getprinc(krb5_context context, kadm5_auth_moddata data,
             krb5_const_principal client, krb5_const_principal target)
{
    return acl_check(data, ACL_INQUIRE, client, target, NULL);
}

static krb5_error_code
acl_getstrs(krb5_context context, kadm5_auth_moddata data,
            krb5_const_principal client, krb5_const_principal target)
{
    return acl_check(data, ACL_INQUIRE, client, target, NULL);
}

static krb5_error_code
acl_extract(krb5_context context, kadm5_auth_moddata data,
            krb5_const_principal client, krb5_const_principal target)
{
    return acl_check(data, ACL_EXTRACT, client, target, NULL);
}

static krb5_error_code
acl_listprincs(krb5_context context, kadm5_auth_moddata data,
               krb5_const_principal client)
{
    return acl_check(data, ACL_LIST, client, NULL, NULL);
}

static krb5_error_code
acl_addpol(krb5_context context, kadm5_auth_moddata data,
           krb5_const_principal client, const char *policy,
           const struct _kadm5_policy_ent_t *ent, long mask)
{
    return acl_check(data, ACL_ADD, client, NULL, NULL);
}

static krb5_error_code
acl_modpol(krb5_context context, kadm5_auth_moddata data,
           krb5_const_principal client, const char *policy,
           const struct _kadm5_policy_ent_t *ent, long mask)
{
    return acl_check(data, ACL_MODIFY, client, NULL, NULL);
}

static krb5_error_code
acl_delpol(krb5_context context, kadm5_auth_moddata data,
           krb5_const_principal client, const char *policy)
{
    return acl_check(data, ACL_DELETE, client, NULL, NULL);
}

static krb5_error_code
acl_getpol(krb5_context context, kadm5_auth_moddata data,
           krb5_const_principal client, const char *policy,
           const char *client_policy)
{
    return acl_check(data, ACL_INQUIRE, client, NULL, NULL);
}

static krb5_error_code
acl_listpols(krb5_context context, kadm5_auth_moddata data,
             krb5_const_principal client)
{
    return acl_check(data, ACL_LIST, client, NULL, NULL);
}

static krb5_error_code
acl_iprop(krb5_context context, kadm5_auth_moddata data,
          krb5_const_principal client)
{
    return acl_check(data, ACL_IPROP, client, NULL, NULL);
}

krb5_error_code
kadm5_auth_acl_initvt(krb5_context context, int maj_ver, int min_ver,
                      krb5_plugin_vtable vtable)
{
    kadm5_auth_vtable vt;

    if (maj_ver != 1)
        return KRB5_PLUGIN_VER_NOTSUPP;
    vt = (kadm5_auth_vtable)vtable;
    vt->name = "acl";
    vt->init = acl_init;
    vt->fini = acl_fini;
    vt->addprinc = acl_addprinc;
    vt->modprinc = acl_modprinc;
    vt->setstr = acl_setstr;
    vt->cpw = acl_cpw;
    vt->chrand = acl_chrand;
    vt->setkey = acl_setkey;
    vt->purgekeys = acl_purgekeys;
    vt->delprinc = acl_delprinc;
    vt->renprinc = acl_renprinc;
    vt->getprinc = acl_getprinc;
    vt->getstrs = acl_getstrs;
    vt->extract = acl_extract;
    vt->listprincs = acl_listprincs;
    vt->addpol = acl_addpol;
    vt->modpol = acl_modpol;
    vt->delpol = acl_delpol;
    vt->getpol = acl_getpol;
    vt->listpols = acl_listpols;
    vt->iprop = acl_iprop;
    return 0;
}
