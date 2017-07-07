/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/kadm5/srv/server_acl.c */
/*
 * Copyright 1995-2004, 2007, 2008 by the Massachusetts Institute of Technology.
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

#include "k5-int.h"
#include <syslog.h>
#include <sys/param.h>
#include <gssapi/gssapi_generic.h>
#include <kadm5/server_internal.h>
#include <kadm5/admin.h>
#include "adm_proto.h"
#include "server_acl.h"
#include <ctype.h>

typedef struct _acl_op_table {
    char        ao_op;
    krb5_int32  ao_mask;
} aop_t;

typedef struct _acl_entry {
    struct _acl_entry   *ae_next;
    char                *ae_name;
    krb5_boolean        ae_name_bad;
    krb5_principal      ae_principal;
    krb5_int32          ae_op_allowed;
    char                *ae_target;
    krb5_boolean        ae_target_bad;
    krb5_principal      ae_target_princ;
    char                *ae_restriction_string;
    /* eg: "-maxlife 3h -service +proxiable" */
    krb5_boolean        ae_restriction_bad;
    restriction_t       *ae_restrictions;
} aent_t;

static const aop_t acl_op_table[] = {
    { 'a',      ACL_ADD },
    { 'd',      ACL_DELETE },
    { 'm',      ACL_MODIFY },
    { 'c',      ACL_CHANGEPW },
    { 'i',      ACL_INQUIRE },
    { 'l',      ACL_LIST },
    { 'p',      ACL_IPROP },
    { 's',      ACL_SETKEY },
    { 'x',      ACL_ALL_MASK },
    { '*',      ACL_ALL_MASK },
    { 'e',      ACL_EXTRACT },
    { '\0',     0 }
};

typedef struct _wildstate {
    int nwild;
    const krb5_data *backref[9];
} wildstate_t;

static aent_t   *acl_list_head = (aent_t *) NULL;
static aent_t   *acl_list_tail = (aent_t *) NULL;

static const char *acl_acl_file = (char *) NULL;
static int acl_inited = 0;
static int acl_debug_level = 0;
/*
 * This is the catchall entry.  If nothing else appropriate is found, or in
 * the case where the ACL file is not present, this entry controls what can
 * be done.
 */
static const char *acl_catchall_entry = NULL;

static const char *acl_line2long_msg = N_("%s: line %d too long, truncated");
static const char *acl_op_bad_msg = N_("Unrecognized ACL operation '%c' in "
                                       "%s");
static const char *acl_syn_err_msg = N_("%s: syntax error at line %d "
                                        "<%10s...>");
static const char *acl_cantopen_msg = N_("%s while opening ACL file %s");

/*
 * kadm5int_acl_get_line() - Get a line from the ACL file.
 *                      Lines ending with \ are continued on the next line
 */
static char *
kadm5int_acl_get_line(fp, lnp)
    FILE        *fp;
    int         *lnp;           /* caller should set to 1 before first call */
{
    int         i, domore;
    static int  line_incr = 0;
    static char acl_buf[BUFSIZ];

    *lnp += line_incr;
    line_incr = 0;
    for (domore = 1; domore && !feof(fp); ) {
        /* Copy in the line, with continuations */
        for (i = 0; ((i < BUFSIZ) && !feof(fp)); i++) {
            int byte;
            byte = fgetc(fp);
            acl_buf[i] = byte;
            if (byte == EOF) {
                if (i > 0 && acl_buf[i-1] == '\\')
                    i--;
                break;          /* it gets nulled-out below */
            }
            else if (acl_buf[i] == '\n') {
                if (i == 0 || acl_buf[i-1] != '\\')
                    break;      /* empty line or normal end of line */
                else {
                    i -= 2;     /* back up over "\\\n" and continue */
                    line_incr++;
                }
            }
        }
        /* Check if we exceeded our buffer size */
        if (i == sizeof acl_buf && (i--, !feof(fp))) {
            int c1 = acl_buf[i], c2;

            krb5_klog_syslog(LOG_ERR, _(acl_line2long_msg), acl_acl_file,
                             *lnp);
            while ((c2 = fgetc(fp)) != EOF) {
                if (c2 == '\n') {
                    if (c1 != '\\')
                        break;
                    line_incr++;
                }
                c1 = c2;
            }
        }
        acl_buf[i] = '\0';
        if (acl_buf[0] == (char) EOF)   /* ptooey */
            acl_buf[0] = '\0';
        else
            line_incr++;
        if ((acl_buf[0] != '#') && (acl_buf[0] != '\0'))
            domore = 0;
    }
    if (domore || (strlen(acl_buf) == 0))
        return((char *) NULL);
    else
        return(acl_buf);
}

/*
 * kadm5int_acl_parse_line() - Parse the contents of an ACL line.
 */
static aent_t *
kadm5int_acl_parse_line(lp)
    const char *lp;
{
    static char acle_principal[BUFSIZ];
    static char acle_ops[BUFSIZ];
    static char acle_object[BUFSIZ];
    static char acle_restrictions[BUFSIZ];
    aent_t      *acle;
    char        *op;
    int         t, found, opok, nmatch;

    DPRINT(DEBUG_CALLS, acl_debug_level,
           ("* kadm5int_acl_parse_line(line=%20s)\n", lp));
    /*
     * Format is still simple:
     *  entry ::= [<whitespace>] <principal> <whitespace> <opstring>
     *            [<whitespace> <target> [<whitespace> <restrictions>
     *                                    [<whitespace>]]]
     */
    acle = (aent_t *) NULL;
    acle_object[0] = '\0';
    nmatch = sscanf(lp, "%s %s %s %[^\n]", acle_principal, acle_ops,
                    acle_object, acle_restrictions);
    if (nmatch >= 2) {
        acle = (aent_t *) malloc(sizeof(aent_t));
        if (acle) {
            acle->ae_next = (aent_t *) NULL;
            acle->ae_op_allowed = (krb5_int32) 0;
            acle->ae_target =
                (nmatch >= 3) ? strdup(acle_object) : (char *) NULL;
            acle->ae_target_bad = 0;
            acle->ae_target_princ = (krb5_principal) NULL;
            opok = 1;
            for (op=acle_ops; *op; op++) {
                char rop;

                rop = (isupper((unsigned char) *op)) ? tolower((unsigned char) *op) : *op;
                found = 0;
                for (t=0; acl_op_table[t].ao_op; t++) {
                    if (rop == acl_op_table[t].ao_op) {
                        found = 1;
                        if (rop == *op)
                            acle->ae_op_allowed |= acl_op_table[t].ao_mask;
                        else
                            acle->ae_op_allowed &= ~acl_op_table[t].ao_mask;
                    }
                }
                if (!found) {
                    krb5_klog_syslog(LOG_ERR, _(acl_op_bad_msg), *op, lp);
                    opok = 0;
                }
            }
            if (opok) {
                acle->ae_name = strdup(acle_principal);
                if (acle->ae_name) {
                    acle->ae_principal = (krb5_principal) NULL;
                    acle->ae_name_bad = 0;
                    DPRINT(DEBUG_ACL, acl_debug_level,
                           ("A ACL entry %s -> opmask %x\n",
                            acle->ae_name, acle->ae_op_allowed));
                }
                else {
                    if (acle->ae_target)
                        free(acle->ae_target);
                    free(acle);
                    acle = (aent_t *) NULL;
                }
            }
            else {
                if (acle->ae_target)
                    free(acle->ae_target);
                free(acle);
                acle = (aent_t *) NULL;
            }

            if (acle) {
                if ( nmatch >= 4 ) {
                    char        *trailing;

                    trailing = &acle_restrictions[strlen(acle_restrictions)-1];
                    while ( isspace((int) *trailing) )
                        trailing--;
                    trailing[1] = '\0';
                    acle->ae_restriction_string =
                        strdup(acle_restrictions);
                }
                else {
                    acle->ae_restriction_string = (char *) NULL;
                }
                acle->ae_restriction_bad = 0;
                acle->ae_restrictions = (restriction_t *) NULL;
            }
        }
    }
    DPRINT(DEBUG_CALLS, acl_debug_level,
           ("X kadm5int_acl_parse_line() = %x\n", (long) acle));
    return(acle);
}

/*
 * kadm5int_acl_parse_restrictions() - Parse optional restrictions field
 *
 * Allowed restrictions are:
 *      [+-]flagname            (recognized by krb5_flagspec_to_mask)
 *                              flag is forced to indicated value
 *      -clearpolicy            policy is forced clear
 *      -policy pol             policy is forced to be "pol"
 *      -{expire,pwexpire,maxlife,maxrenewlife} deltat
 *                              associated value will be forced to
 *                              MIN(deltat, requested value)
 *
 * Returns: 0 on success, or system errors
 */
static krb5_error_code
kadm5int_acl_parse_restrictions(s, rpp)
    char                *s;
    restriction_t       **rpp;
{
    char                *sp = NULL, *tp, *ap, *save;
    static const char   *delims = "\t\n\f\v\r ,";
    krb5_deltat         dt;
    krb5_error_code     code;

    DPRINT(DEBUG_CALLS, acl_debug_level,
           ("* kadm5int_acl_parse_restrictions(s=%20s, rpp=0x%08x)\n", s, (long)rpp));

    *rpp = (restriction_t *) NULL;
    code = 0;
    if (s) {
        if (!(sp = strdup(s))   /* Don't munge the original */
            || !(*rpp = (restriction_t *) malloc(sizeof(restriction_t)))) {
            code = ENOMEM;
        } else {
            memset(*rpp, 0, sizeof(**rpp));
            (*rpp)->forbid_attrs = ~(krb5_flags)0;
            for (tp = strtok_r(sp, delims, &save); tp;
                 tp = strtok_r(NULL, delims, &save)) {
                if (!krb5_flagspec_to_mask(tp, &(*rpp)->require_attrs,
                                           &(*rpp)->forbid_attrs)) {
                    (*rpp)->mask |= KADM5_ATTRIBUTES;
                } else if (!strcmp(tp, "-clearpolicy")) {
                    (*rpp)->mask |= KADM5_POLICY_CLR;
                } else {
                    /* everything else needs an argument ... */
                    if (!(ap = strtok_r(NULL, delims, &save))) {
                        code = EINVAL;
                        break;
                    }
                    if (!strcmp(tp, "-policy")) {
                        if (!((*rpp)->policy = strdup(ap))) {
                            code = ENOMEM;
                            break;
                        }
                        (*rpp)->mask |= KADM5_POLICY;
                    } else {
                        /* all other arguments must be a deltat ... */
                        if (krb5_string_to_deltat(ap, &dt)) {
                            code = EINVAL;
                            break;
                        }
                        if (!strcmp(tp, "-expire")) {
                            (*rpp)->princ_lifetime = dt;
                            (*rpp)->mask |= KADM5_PRINC_EXPIRE_TIME;
                        } else if (!strcmp(tp, "-pwexpire")) {
                            (*rpp)->pw_lifetime = dt;
                            (*rpp)->mask |= KADM5_PW_EXPIRATION;
                        } else if (!strcmp(tp, "-maxlife")) {
                            (*rpp)->max_life = dt;
                            (*rpp)->mask |= KADM5_MAX_LIFE;
                        } else if (!strcmp(tp, "-maxrenewlife")) {
                            (*rpp)->max_renewable_life = dt;
                            (*rpp)->mask |= KADM5_MAX_RLIFE;
                        } else {
                            code = EINVAL;
                            break;
                        }
                    }
                }
            }
            if (code) {
                krb5_klog_syslog(LOG_ERR, _("%s: invalid restrictions: %s"),
                                 acl_acl_file, s);
            }
        }
    }
    if (sp)
        free(sp);
    if (*rpp && code) {
        if ((*rpp)->policy)
            free((*rpp)->policy);
        free(*rpp);
        *rpp = (restriction_t *) NULL;
    }
    DPRINT(DEBUG_CALLS, acl_debug_level,
           ("X kadm5int_acl_parse_restrictions() = %d, mask=0x%08x\n",
            code, (*rpp) ? (*rpp)->mask : 0));
    return code;
}

/*
 * kadm5int_acl_impose_restrictions()   - impose restrictions, modifying *recp, *maskp
 *
 * Returns: 0 on success;
 *          malloc or timeofday errors
 */
krb5_error_code
kadm5int_acl_impose_restrictions(kcontext, recp, maskp, rp)
    krb5_context               kcontext;
    kadm5_principal_ent_rec    *recp;
    long                       *maskp;
    restriction_t              *rp;
{
    krb5_error_code     code;
    krb5_int32          now;

    DPRINT(DEBUG_CALLS, acl_debug_level,
           ("* kadm5int_acl_impose_restrictions(..., *maskp=0x%08x, rp=0x%08x)\n",
            *maskp, (long)rp));
    if (!rp)
        return 0;
    if (rp->mask & (KADM5_PRINC_EXPIRE_TIME|KADM5_PW_EXPIRATION))
        if ((code = krb5_timeofday(kcontext, &now)))
            return code;

    if (rp->mask & KADM5_ATTRIBUTES) {
        recp->attributes |= rp->require_attrs;
        recp->attributes &= rp->forbid_attrs;
        *maskp |= KADM5_ATTRIBUTES;
    }
    if (rp->mask & KADM5_POLICY_CLR) {
        *maskp &= ~KADM5_POLICY;
        *maskp |= KADM5_POLICY_CLR;
    } else if (rp->mask & KADM5_POLICY) {
        if (recp->policy && strcmp(recp->policy, rp->policy)) {
            free(recp->policy);
            recp->policy = (char *) NULL;
        }
        if (!recp->policy) {
            recp->policy = strdup(rp->policy);  /* XDR will free it */
            if (!recp->policy)
                return ENOMEM;
        }
        *maskp |= KADM5_POLICY;
    }
    if (rp->mask & KADM5_PRINC_EXPIRE_TIME) {
        if (!(*maskp & KADM5_PRINC_EXPIRE_TIME)
            || (recp->princ_expire_time > (now + rp->princ_lifetime)))
            recp->princ_expire_time = now + rp->princ_lifetime;
        *maskp |= KADM5_PRINC_EXPIRE_TIME;
    }
    if (rp->mask & KADM5_PW_EXPIRATION) {
        if (!(*maskp & KADM5_PW_EXPIRATION)
            || (recp->pw_expiration > (now + rp->pw_lifetime)))
            recp->pw_expiration = now + rp->pw_lifetime;
        *maskp |= KADM5_PW_EXPIRATION;
    }
    if (rp->mask & KADM5_MAX_LIFE) {
        if (!(*maskp & KADM5_MAX_LIFE)
            || (recp->max_life > rp->max_life))
            recp->max_life = rp->max_life;
        *maskp |= KADM5_MAX_LIFE;
    }
    if (rp->mask & KADM5_MAX_RLIFE) {
        if (!(*maskp & KADM5_MAX_RLIFE)
            || (recp->max_renewable_life > rp->max_renewable_life))
            recp->max_renewable_life = rp->max_renewable_life;
        *maskp |= KADM5_MAX_RLIFE;
    }
    DPRINT(DEBUG_CALLS, acl_debug_level,
           ("X kadm5int_acl_impose_restrictions() = 0, *maskp=0x%08x\n", *maskp));
    return 0;
}

/*
 * kadm5int_acl_free_entries() - Free all ACL entries.
 */
static void
kadm5int_acl_free_entries()
{
    aent_t      *ap;
    aent_t      *np;

    DPRINT(DEBUG_CALLS, acl_debug_level, ("* kadm5int_acl_free_entries()\n"));
    for (ap=acl_list_head; ap; ap = np) {
        if (ap->ae_name)
            free(ap->ae_name);
        if (ap->ae_principal)
            krb5_free_principal((krb5_context) NULL, ap->ae_principal);
        if (ap->ae_target)
            free(ap->ae_target);
        if (ap->ae_target_princ)
            krb5_free_principal((krb5_context) NULL, ap->ae_target_princ);
        if (ap->ae_restriction_string)
            free(ap->ae_restriction_string);
        if (ap->ae_restrictions) {
            if (ap->ae_restrictions->policy)
                free(ap->ae_restrictions->policy);
            free(ap->ae_restrictions);
        }
        np = ap->ae_next;
        free(ap);
    }
    acl_list_head = acl_list_tail = (aent_t *) NULL;
    acl_inited = 0;
    DPRINT(DEBUG_CALLS, acl_debug_level, ("X kadm5int_acl_free_entries()\n"));
}

/*
 * kadm5int_acl_load_acl_file() - Open and parse the ACL file.
 */
static int
kadm5int_acl_load_acl_file()
{
    FILE        *afp;
    char        *alinep;
    aent_t      **aentpp;
    int         alineno;
    int         retval = 1;

    DPRINT(DEBUG_CALLS, acl_debug_level, ("* kadm5int_acl_load_acl_file()\n"));
    /* Open the ACL file for read */
    afp = fopen(acl_acl_file, "r");
    if (afp) {
        set_cloexec_file(afp);
        alineno = 1;
        aentpp = &acl_list_head;

        /* Get a non-comment line */
        while ((alinep = kadm5int_acl_get_line(afp, &alineno))) {
            /* Parse it */
            *aentpp = kadm5int_acl_parse_line(alinep);
            /* If syntax error, then fall out */
            if (!*aentpp) {
                krb5_klog_syslog(LOG_ERR, _(acl_syn_err_msg),
                                 acl_acl_file, alineno, alinep);
                retval = 0;
                break;
            }
            acl_list_tail = *aentpp;
            aentpp = &(*aentpp)->ae_next;
        }

        fclose(afp);

        if (acl_catchall_entry) {
            *aentpp = kadm5int_acl_parse_line(acl_catchall_entry);
            if (*aentpp) {
                acl_list_tail = *aentpp;
            }
            else {
                retval = 0;
                DPRINT(DEBUG_OPERATION, acl_debug_level,
                       ("> catchall acl entry (%s) load failed\n",
                        acl_catchall_entry));
            }
        }
    }
    else {
        krb5_klog_syslog(LOG_ERR, _(acl_cantopen_msg),
                         error_message(errno), acl_acl_file);
        if (acl_catchall_entry &&
            (acl_list_head = kadm5int_acl_parse_line(acl_catchall_entry))) {
            acl_list_tail = acl_list_head;
        }
        else {
            retval = 0;
            DPRINT(DEBUG_OPERATION, acl_debug_level,
                   ("> catchall acl entry (%s) load failed\n",
                    acl_catchall_entry));
        }
    }

    if (!retval) {
        kadm5int_acl_free_entries();
    }
    DPRINT(DEBUG_CALLS, acl_debug_level,
           ("X kadm5int_acl_load_acl_file() = %d\n", retval));
    return(retval);
}

/*
 * kadm5int_acl_match_data()    - See if two data entries match.
 *
 * Wildcarding is only supported for a whole component.
 */
static krb5_boolean
kadm5int_acl_match_data(const krb5_data *e1, const krb5_data *e2,
                        int targetflag, wildstate_t *ws)
{
    krb5_boolean        retval;

    DPRINT(DEBUG_CALLS, acl_debug_level,
           ("* acl_match_entry(%s, %s)\n", e1->data, e2->data));
    retval = 0;
    if (!strncmp(e1->data, "*", e1->length)) {
        retval = 1;
        if (ws && !targetflag) {
            if (ws->nwild >= 9) {
                DPRINT(DEBUG_ACL, acl_debug_level,
                       ("Too many wildcards in ACL entry.\n"));
            }
            else
                ws->backref[ws->nwild++] = e2;
        }
    }
    else if (ws && targetflag && (e1->length == 2) && (e1->data[0] == '*') &&
             (e1->data[1] >= '1') && (e1->data[1] <= '9')) {
        int     n = e1->data[1] - '1';
        if (n >= ws->nwild) {
            DPRINT(DEBUG_ACL, acl_debug_level,
                   ("Too many backrefs in ACL entry.\n"));
        }
        else if ((ws->backref[n]->length == e2->length) &&
                 (!strncmp(ws->backref[n]->data, e2->data, e2->length)))
            retval = 1;

    }
    else {
        if ((e1->length == e2->length) &&
            (!strncmp(e1->data, e2->data, e1->length)))
            retval = 1;
    }
    DPRINT(DEBUG_CALLS, acl_debug_level, ("X acl_match_entry()=%d\n",retval));
    return(retval);
}

/*
 * kadm5int_acl_find_entry()    - Find a matching entry.
 */
static aent_t *
kadm5int_acl_find_entry(krb5_context kcontext, krb5_const_principal principal,
                        krb5_const_principal dest_princ)
{
    aent_t              *entry;
    krb5_error_code     kret;
    int                 i;
    int                 matchgood;
    wildstate_t         state;

    DPRINT(DEBUG_CALLS, acl_debug_level, ("* kadm5int_acl_find_entry()\n"));
    for (entry=acl_list_head; entry; entry = entry->ae_next) {
        memset(&state, 0, sizeof(state));
        if (entry->ae_name_bad)
            continue;
        if (!strcmp(entry->ae_name, "*")) {
            DPRINT(DEBUG_ACL, acl_debug_level, ("A wildcard ACL match\n"));
            matchgood = 1;
        }
        else {
            if (!entry->ae_principal && !entry->ae_name_bad) {
                kret = krb5_parse_name(kcontext,
                                       entry->ae_name,
                                       &entry->ae_principal);
                if (kret)
                    entry->ae_name_bad = 1;
            }
            if (entry->ae_name_bad) {
                DPRINT(DEBUG_ACL, acl_debug_level,
                       ("Bad ACL entry %s\n", entry->ae_name));
                continue;
            }
            matchgood = 0;
            if (kadm5int_acl_match_data(&entry->ae_principal->realm,
                                        &principal->realm, 0, (wildstate_t *)0) &&
                (entry->ae_principal->length == principal->length)) {
                matchgood = 1;
                for (i=0; i<principal->length; i++) {
                    if (!kadm5int_acl_match_data(&entry->ae_principal->data[i],
                                                 &principal->data[i], 0, &state)) {
                        matchgood = 0;
                        break;
                    }
                }
            }
        }
        if (!matchgood)
            continue;

        /* We've matched the principal.  If we have a target, then try it */
        if (entry->ae_target && strcmp(entry->ae_target, "*")) {
            if (!entry->ae_target_princ && !entry->ae_target_bad) {
                kret = krb5_parse_name(kcontext, entry->ae_target,
                                       &entry->ae_target_princ);
                if (kret)
                    entry->ae_target_bad = 1;
            }
            if (entry->ae_target_bad) {
                DPRINT(DEBUG_ACL, acl_debug_level,
                       ("Bad target in ACL entry for %s\n", entry->ae_name));
                entry->ae_name_bad = 1;
                continue;
            }
            if (!dest_princ)
                matchgood = 0;
            else if (entry->ae_target_princ && dest_princ) {
                if (kadm5int_acl_match_data(&entry->ae_target_princ->realm,
                                            &dest_princ->realm, 1, (wildstate_t *)0) &&
                    (entry->ae_target_princ->length == dest_princ->length)) {
                    for (i=0; i<dest_princ->length; i++) {
                        if (!kadm5int_acl_match_data(&entry->ae_target_princ->data[i],
                                                     &dest_princ->data[i], 1, &state)) {
                            matchgood = 0;
                            break;
                        }
                    }
                }
                else
                    matchgood = 0;
            }
        }
        if (!matchgood)
            continue;

        if (entry->ae_restriction_string
            && !entry->ae_restriction_bad
            && !entry->ae_restrictions
            && kadm5int_acl_parse_restrictions(entry->ae_restriction_string,
                                               &entry->ae_restrictions)) {
            DPRINT(DEBUG_ACL, acl_debug_level,
                   ("Bad restrictions in ACL entry for %s\n", entry->ae_name));
            entry->ae_restriction_bad = 1;
        }
        if (entry->ae_restriction_bad) {
            entry->ae_name_bad = 1;
            continue;
        }
        break;
    }
    DPRINT(DEBUG_CALLS, acl_debug_level, ("X kadm5int_acl_find_entry()=%x\n",entry));
    return(entry);
}

/*
 * kadm5int_acl_init()  - Initialize ACL context.
 */
krb5_error_code
kadm5int_acl_init(kcontext, debug_level, acl_file)
    krb5_context        kcontext;
    int                 debug_level;
    char                *acl_file;
{
    krb5_error_code     kret;

    kret = 0;
    acl_debug_level = debug_level;
    DPRINT(DEBUG_CALLS, acl_debug_level,
           ("* kadm5int_acl_init(afile=%s)\n",
            ((acl_file) ? acl_file : "(null)")));
    acl_acl_file = (acl_file) ? acl_file : (char *) KRB5_DEFAULT_ADMIN_ACL;
    acl_inited = kadm5int_acl_load_acl_file();

    DPRINT(DEBUG_CALLS, acl_debug_level, ("X kadm5int_acl_init() = %d\n", kret));
    return(kret);
}

/*
 * kadm5int_acl_finish  - Terminate ACL context.
 */
void
kadm5int_acl_finish(kcontext, debug_level)
    krb5_context        kcontext;
    int                 debug_level;
{
    DPRINT(DEBUG_CALLS, acl_debug_level, ("* kadm5int_acl_finish()\n"));
    kadm5int_acl_free_entries();
    DPRINT(DEBUG_CALLS, acl_debug_level, ("X kadm5int_acl_finish()\n"));
}

/*
 * kadm5int_acl_check_krb()     - Is this operation permitted for this principal?
 */
krb5_boolean
kadm5int_acl_check_krb(kcontext, caller_princ, opmask, principal, restrictions)
    krb5_context         kcontext;
    krb5_const_principal caller_princ;
    krb5_int32           opmask;
    krb5_const_principal principal;
    restriction_t        **restrictions;
{
    krb5_boolean        retval;
    aent_t              *aentry;

    DPRINT(DEBUG_CALLS, acl_debug_level, ("* acl_op_permitted()\n"));

    retval = FALSE;

    aentry = kadm5int_acl_find_entry(kcontext, caller_princ, principal);
    if (aentry) {
        if ((aentry->ae_op_allowed & opmask) == opmask) {
            retval = TRUE;
            if (restrictions) {
                *restrictions =
                    (aentry->ae_restrictions && aentry->ae_restrictions->mask)
                    ? aentry->ae_restrictions
                    : (restriction_t *) NULL;
            }
        }
    }

    DPRINT(DEBUG_CALLS, acl_debug_level, ("X acl_op_permitted()=%d\n",
                                          retval));
    return retval;
}

/*
 * kadm5int_acl_check() - Is this operation permitted for this principal?
 *                      this code used not to be based on gssapi.  In order
 *                      to minimize porting hassles, I've put all the
 *                      gssapi hair in this function.  This might not be
 *                      the best medium-term solution.  (The best long-term
 *                      solution is, of course, a real authorization service.)
 */
krb5_boolean
kadm5int_acl_check(kcontext, caller, opmask, principal, restrictions)
    krb5_context        kcontext;
    gss_name_t          caller;
    krb5_int32          opmask;
    krb5_principal      principal;
    restriction_t       **restrictions;
{
    krb5_boolean        retval;
    gss_buffer_desc     caller_buf;
    gss_OID             caller_oid;
    OM_uint32           emin;
    krb5_error_code     code;
    krb5_principal      caller_princ;

    if (GSS_ERROR(gss_display_name(&emin, caller, &caller_buf, &caller_oid)))
        return FALSE;

    code = krb5_parse_name(kcontext, (char *) caller_buf.value,
                           &caller_princ);

    gss_release_buffer(&emin, &caller_buf);

    if (code != 0)
        return FALSE;

    retval = kadm5int_acl_check_krb(kcontext, caller_princ,
                                    opmask, principal, restrictions);

    krb5_free_principal(kcontext, caller_princ);

    return retval;
}

kadm5_ret_t
kadm5_get_privs(void *server_handle, long *privs)
{
    CHECK_HANDLE(server_handle);

    /* this is impossible to do with the current interface.  For now,
       return all privs, which will confuse some clients, but not
       deny any access to users of "smart" clients which try to cache */

    *privs = ~0;

    return KADM5_OK;
}
