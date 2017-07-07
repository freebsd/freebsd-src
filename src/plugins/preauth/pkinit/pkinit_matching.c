/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * COPYRIGHT (C) 2007
 * THE REGENTS OF THE UNIVERSITY OF MICHIGAN
 * ALL RIGHTS RESERVED
 *
 * Permission is granted to use, copy, create derivative works
 * and redistribute this software and such derivative works
 * for any purpose, so long as the name of The University of
 * Michigan is not used in any advertising or publicity
 * pertaining to the use of distribution of this software
 * without specific, written prior authorization.  If the
 * above copyright notice or any other identification of the
 * University of Michigan is included in any copy of any
 * portion of this software, then the disclaimer below must
 * also be included.
 *
 * THIS SOFTWARE IS PROVIDED AS IS, WITHOUT REPRESENTATION
 * FROM THE UNIVERSITY OF MICHIGAN AS TO ITS FITNESS FOR ANY
 * PURPOSE, AND WITHOUT WARRANTY BY THE UNIVERSITY OF
 * MICHIGAN OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING
 * WITHOUT LIMITATION THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. THE
 * REGENTS OF THE UNIVERSITY OF MICHIGAN SHALL NOT BE LIABLE
 * FOR ANY DAMAGES, INCLUDING SPECIAL, INDIRECT, INCIDENTAL, OR
 * CONSEQUENTIAL DAMAGES, WITH RESPECT TO ANY CLAIM ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OF THE SOFTWARE, EVEN
 * IF IT HAS BEEN OR IS HEREAFTER ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGES.
 */

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <regex.h>
#include "pkinit.h"

typedef struct _pkinit_cert_info pkinit_cert_info;

typedef enum {
    kw_undefined = 0,
    kw_subject = 1,
    kw_issuer = 2,
    kw_san = 3,
    kw_eku = 4,
    kw_ku = 5
} keyword_type;

static char *
keyword2string(unsigned int kw)
{
    switch(kw) {
    case kw_undefined: return "NONE"; break;
    case kw_subject: return "SUBJECT"; break;
    case kw_issuer: return "ISSUER"; break;
    case kw_san: return "SAN"; break;
    case kw_eku: return "EKU"; break;
    case kw_ku: return "KU"; break;
    default: return "INVALID"; break;
    }
}
typedef enum {
    relation_none = 0,
    relation_and = 1,
    relation_or = 2
} relation_type;

static char *
relation2string(unsigned int rel)
{
    switch(rel) {
    case relation_none: return "NONE"; break;
    case relation_and: return "AND"; break;
    case relation_or: return "OR"; break;
    default: return "INVALID"; break;
    }
}

typedef enum {
    kwvaltype_undefined = 0,
    kwvaltype_regexp = 1,
    kwvaltype_list = 2
} kw_value_type;

static char *
kwval2string(unsigned int kwval)
{
    switch(kwval) {
    case kwvaltype_undefined: return "NONE"; break;
    case kwvaltype_regexp: return "REGEXP"; break;
    case kwvaltype_list: return "LIST"; break;
    default: return "INVALID"; break;
    }
}

struct keyword_desc {
    const char *value;
    size_t length;
    keyword_type kwtype;
    kw_value_type kwvaltype;
} matching_keywords[] = {
    { "<KU>",       4, kw_ku, kwvaltype_list },
    { "<EKU>",      5, kw_eku, kwvaltype_list },
    { "<SAN>",      5, kw_san, kwvaltype_regexp },
    { "<ISSUER>",   8, kw_issuer, kwvaltype_regexp },
    { "<SUBJECT>",  9, kw_subject, kwvaltype_regexp },
    { NULL, 0, kw_undefined, kwvaltype_undefined},
};

struct ku_desc {
    const char *value;
    size_t length;
    unsigned int bitval;
};

struct ku_desc ku_keywords[] = {
    { "digitalSignature",   16, PKINIT_KU_DIGITALSIGNATURE },
    { "keyEncipherment",    15, PKINIT_KU_KEYENCIPHERMENT },
    { NULL, 0, 0 },
};

struct ku_desc  eku_keywords[] = {
    { "pkinit",             6,  PKINIT_EKU_PKINIT },
    { "msScLogin",          9,  PKINIT_EKU_MSSCLOGIN },
    { "clientAuth",         10, PKINIT_EKU_CLIENTAUTH },
    { "emailProtection",    15, PKINIT_EKU_EMAILPROTECTION },
    { NULL, 0, 0 },
};

/* Rule component */
typedef struct _rule_component {
    struct _rule_component *next;
    keyword_type kw_type;
    kw_value_type kwval_type;
    regex_t regexp;         /* Compiled regular expression */
    char *regsrc;           /* The regular expression source (for debugging) */
    unsigned int ku_bits;
    unsigned int eku_bits;
} rule_component;

/* Set rule components */
typedef struct _rule_set {
    relation_type relation;
    int num_crs;
    rule_component *crs;
} rule_set;

static krb5_error_code
free_rule_component(krb5_context context,
                    rule_component *rc)
{
    if (rc == NULL)
        return 0;

    if (rc->kwval_type == kwvaltype_regexp) {
        free(rc->regsrc);
        regfree(&rc->regexp);
    }
    free(rc);
    return 0;
}

static krb5_error_code
free_rule_set(krb5_context context,
              rule_set *rs)
{
    rule_component *rc, *trc;

    if (rs == NULL)
        return 0;
    for (rc = rs->crs; rc != NULL;) {
        trc = rc->next;
        free_rule_component(context, rc);
        rc = trc;
    }
    free(rs);
    return 0;
}

static krb5_error_code
parse_list_value(krb5_context context,
                 keyword_type type,
                 char *value,
                 rule_component *rc)
{
    krb5_error_code retval;
    char *comma;
    struct ku_desc *ku = NULL;
    int found;
    size_t len;
    unsigned int *bitptr;


    if (value == NULL || value[0] == '\0') {
        pkiDebug("%s: Missing or empty value for list keyword type %d\n",
                 __FUNCTION__, type);
        retval = EINVAL;
        goto out;
    }

    if (type == kw_eku) {
        bitptr = &rc->eku_bits;
    } else if (type == kw_ku) {
        bitptr = &rc->ku_bits;
    } else {
        pkiDebug("%s: Unknown list keyword type %d\n", __FUNCTION__, type);
        retval = EINVAL;
        goto out;
    }

    do {
        found = 0;
        comma = strchr(value, ',');
        if (comma != NULL)
            len = comma - value;
        else
            len = strlen(value);

        if (type == kw_eku) {
            ku = eku_keywords;
        } else if (type == kw_ku) {
            ku = ku_keywords;
        }

        for (; ku->value != NULL; ku++) {
            if (strncasecmp(value, ku->value, len) == 0) {
                *bitptr |= ku->bitval;
                found = 1;
                pkiDebug("%s: Found value '%s', bitfield is now 0x%x\n",
                         __FUNCTION__, ku->value, *bitptr);
                break;
            }
        }
        if (found) {
            value += ku->length;
            if (*value == ',')
                value += 1;
        } else {
            pkiDebug("%s: Urecognized value '%s'\n", __FUNCTION__, value);
            retval = EINVAL;
            goto out;
        }
    } while (found && *value != '\0');

    retval = 0;
out:
    pkiDebug("%s: returning %d\n", __FUNCTION__, retval);
    return retval;
}

static krb5_error_code
parse_rule_component(krb5_context context,
                     const char **rule,
                     int *remaining,
                     rule_component **ret_rule)
{
    krb5_error_code retval;
    rule_component *rc = NULL;
    keyword_type kw_type;
    kw_value_type kwval_type;
    char err_buf[128];
    int ret;
    struct keyword_desc *kw, *nextkw;
    char *nk;
    int found_next_kw = 0;
    char *value = NULL;
    size_t len;

    for (kw = matching_keywords; kw->value != NULL; kw++) {
        if (strncmp(*rule, kw->value, kw->length) == 0) {
            kw_type = kw->kwtype;
            kwval_type = kw->kwvaltype;
            *rule += kw->length;
            *remaining -= kw->length;
            break;
        }
    }
    if (kw->value == NULL) {
        pkiDebug("%s: Missing or invalid keyword in rule '%s'\n",
                 __FUNCTION__, *rule);
        retval = ENOENT;
        goto out;
    }

    pkiDebug("%s: found keyword '%s'\n", __FUNCTION__, kw->value);

    rc = calloc(1, sizeof(*rc));
    if (rc == NULL) {
        retval = ENOMEM;
        goto out;
    }
    rc->next = NULL;
    rc->kw_type = kw_type;
    rc->kwval_type = kwval_type;

    /*
     * Before procesing the value for this keyword,
     * (compiling the regular expression or processing the list)
     * we need to find the end of it.  That means parsing for the
     * beginning of the next keyword (or the end of the rule).
     */
    nk = strchr(*rule, '<');
    while (nk != NULL) {
        /* Possibly another keyword, check it out */
        for (nextkw = matching_keywords; nextkw->value != NULL; nextkw++) {
            if (strncmp(nk, nextkw->value, nextkw->length) == 0) {
                /* Found a keyword, nk points to the beginning */
                found_next_kw = 1;
                break;  /* Need to break out of the while! */
            }
        }
        if (!found_next_kw)
            nk = strchr(nk+1, '<');     /* keep looking */
        else
            break;
    }

    if (nk != NULL && found_next_kw)
        len = (nk - *rule);
    else
        len = (*remaining);

    if (len == 0) {
        pkiDebug("%s: Missing value for keyword '%s'\n",
                 __FUNCTION__, kw->value);
        retval = EINVAL;
        goto out;
    }

    value = calloc(1, len+1);
    if (value == NULL) {
        retval = ENOMEM;
        goto out;
    }
    memcpy(value, *rule, len);
    *remaining -= len;
    *rule += len;
    pkiDebug("%s: found value '%s'\n", __FUNCTION__, value);

    if (kw->kwvaltype == kwvaltype_regexp) {
        ret = regcomp(&rc->regexp, value, REG_EXTENDED);
        if (ret) {
            regerror(ret, &rc->regexp, err_buf, sizeof(err_buf));
            pkiDebug("%s: Error compiling reg-exp '%s': %s\n",
                     __FUNCTION__, value, err_buf);
            retval = ret;
            goto out;
        }
        rc->regsrc = strdup(value);
        if (rc->regsrc == NULL) {
            retval = ENOMEM;
            goto out;
        }
    } else if (kw->kwvaltype == kwvaltype_list) {
        retval = parse_list_value(context, rc->kw_type, value, rc);
        if (retval) {
            pkiDebug("%s: Error %d, parsing list values for keyword %s\n",
                     __FUNCTION__, retval, kw->value);
            goto out;
        }
    }

    *ret_rule = rc;
    retval = 0;
out:
    free(value);
    if (retval && rc != NULL)
        free_rule_component(context, rc);
    pkiDebug("%s: returning %d\n", __FUNCTION__, retval);
    return retval;
}

static krb5_error_code
parse_rule_set(krb5_context context,
               const char *rule_in,
               rule_set **out_rs)
{
    const char *rule;
    int remaining;
    krb5_error_code ret, retval;
    rule_component *rc = NULL, *trc;
    rule_set *rs;


    if (rule_in == NULL)
        return EINVAL;
    rule = rule_in;
    remaining = strlen(rule);

    rs = calloc(1, sizeof(*rs));
    if (rs == NULL) {
        retval = ENOMEM;
        goto cleanup;
    }

    rs->relation = relation_none;
    if (remaining > 1) {
        if (rule[0] == '&' && rule[1] == '&') {
            rs->relation = relation_and;
            rule += 2;
            remaining -= 2;
        } else if (rule_in[0] == '|' && rule_in[1] == '|') {
            rs->relation = relation_or;
            rule +=2;
            remaining -= 2;
        }
    }
    rs->num_crs = 0;
    while (remaining > 0) {
        if (rs->relation == relation_none && rs->num_crs > 0) {
            pkiDebug("%s: Assuming AND relation for multiple components in rule '%s'\n",
                     __FUNCTION__, rule_in);
            rs->relation = relation_and;
        }
        ret = parse_rule_component(context, &rule, &remaining, &rc);
        if (ret) {
            retval = ret;
            goto cleanup;
        }
        pkiDebug("%s: After parse_rule_component, remaining %d, rule '%s'\n",
                 __FUNCTION__, remaining, rule);
        rs->num_crs++;

        /*
         * Chain the new component on the end (order matters since
         * we can short-circuit an OR or an AND relation if an
         * earlier check passes
         */
        for (trc = rs->crs; trc != NULL && trc->next != NULL; trc = trc->next);
        if (trc == NULL)
            rs->crs = rc;
        else {
            trc->next = rc;
        }
    }

    *out_rs = rs;

    retval = 0;
cleanup:
    if (retval && rs != NULL) {
        free_rule_set(context, rs);
    }
    pkiDebug("%s: returning %d\n", __FUNCTION__, retval);
    return retval;
}

static int
regexp_match(krb5_context context, rule_component *rc, char *value)
{
    int code;

    pkiDebug("%s: checking %s rule '%s' with value '%s'\n",
             __FUNCTION__, keyword2string(rc->kw_type), rc->regsrc, value);

    code = regexec(&rc->regexp, value, 0, NULL, 0);

    pkiDebug("%s: the result is%s a match\n", __FUNCTION__,
             code == REG_NOMATCH ? " NOT" : "");

    return (code == 0 ? 1: 0);
}

static int
component_match(krb5_context context,
                rule_component *rc,
                pkinit_cert_matching_data *md)
{
    int match = 0;
    int i;
    krb5_principal p;
    char *princ_string;

    switch (rc->kwval_type) {
    case kwvaltype_regexp:
        switch (rc->kw_type) {
        case kw_subject:
            match = regexp_match(context, rc, md->subject_dn);
            break;
        case kw_issuer:
            match = regexp_match(context, rc, md->issuer_dn);
            break;
        case kw_san:
            if (md->sans == NULL)
                break;
            for (i = 0, p = md->sans[i]; p != NULL; p = md->sans[++i]) {
                krb5_unparse_name(context, p, &princ_string);
                match = regexp_match(context, rc, princ_string);
                krb5_free_unparsed_name(context, princ_string);
                if (match)
                    break;
            }
            break;
        default:
            pkiDebug("%s: keyword %s, keyword value %s mismatch\n",
                     __FUNCTION__, keyword2string(rc->kw_type),
                     kwval2string(kwvaltype_regexp));
            break;
        }
        break;
    case kwvaltype_list:
        switch(rc->kw_type) {
        case kw_eku:
            pkiDebug("%s: checking %s: rule 0x%08x, cert 0x%08x\n",
                     __FUNCTION__, keyword2string(rc->kw_type),
                     rc->eku_bits, md->eku_bits);
            if ((rc->eku_bits & md->eku_bits) == rc->eku_bits)
                match = 1;
            break;
        case kw_ku:
            pkiDebug("%s: checking %s: rule 0x%08x, cert 0x%08x\n",
                     __FUNCTION__, keyword2string(rc->kw_type),
                     rc->ku_bits, md->ku_bits);
            if ((rc->ku_bits & md->ku_bits) == rc->ku_bits)
                match = 1;
            break;
        default:
            pkiDebug("%s: keyword %s, keyword value %s mismatch\n",
                     __FUNCTION__, keyword2string(rc->kw_type),
                     kwval2string(kwvaltype_regexp));
            break;
        }
        break;
    default:
        pkiDebug("%s: unknown keyword value type %d\n",
                 __FUNCTION__, rc->kwval_type);
        break;
    }
    pkiDebug("%s: returning match = %d\n", __FUNCTION__, match);
    return match;
}
/*
 * Returns match_found == 1 only if exactly one certificate matches
 * the given rule
 */
static krb5_error_code
check_all_certs(krb5_context context,
                pkinit_plg_crypto_context plg_cryptoctx,
                pkinit_req_crypto_context req_cryptoctx,
                pkinit_identity_crypto_context id_cryptoctx,
                krb5_principal princ,
                rule_set *rs,   /* rule to check */
                pkinit_cert_matching_data **matchdata,
                int *match_found,
                pkinit_cert_matching_data **matching_cert)
{
    krb5_error_code retval;
    pkinit_cert_matching_data *md;
    int i;
    int comp_match = 0;
    int total_cert_matches = 0;
    rule_component *rc;
    int certs_checked = 0;
    pkinit_cert_matching_data *save_match = NULL;

    if (match_found == NULL || matching_cert == NULL)
        return EINVAL;

    *matching_cert = NULL;
    *match_found = 0;

    pkiDebug("%s: matching rule relation is %s with %d components\n",
             __FUNCTION__, relation2string(rs->relation), rs->num_crs);

    /*
     * Loop through all the certs available and count
     * how many match the rule
     */
    for (i = 0, md = matchdata[i]; md != NULL; md = matchdata[++i]) {
        pkiDebug("%s: subject: '%s'\n", __FUNCTION__, md->subject_dn);
#if 0
        pkiDebug("%s: issuer:  '%s'\n", __FUNCTION__, md->subject_dn);
        for (j = 0, p = md->sans[j]; p != NULL; p = md->sans[++j]) {
            char *san_string;
            krb5_unparse_name(context, p, &san_string);
            pkiDebug("%s: san: '%s'\n", __FUNCTION__, san_string);
            krb5_free_unparsed_name(context, san_string);
        }
#endif
        certs_checked++;
        for (rc = rs->crs; rc != NULL; rc = rc->next) {
            comp_match = component_match(context, rc, md);
            if (comp_match) {
                pkiDebug("%s: match for keyword type %s\n",
                         __FUNCTION__, keyword2string(rc->kw_type));
            }
            if (comp_match && rs->relation == relation_or) {
                pkiDebug("%s: cert matches rule (OR relation)\n",
                         __FUNCTION__);
                total_cert_matches++;
                save_match = md;
                goto nextcert;
            }
            if (!comp_match && rs->relation == relation_and) {
                pkiDebug("%s: cert does not match rule (AND relation)\n",
                         __FUNCTION__);
                goto nextcert;
            }
        }
        if (rc == NULL && comp_match) {
            pkiDebug("%s: cert matches rule (AND relation)\n", __FUNCTION__);
            total_cert_matches++;
            save_match = md;
        }
    nextcert:
        continue;
    }
    pkiDebug("%s: After checking %d certs, we found %d matches\n",
             __FUNCTION__, certs_checked, total_cert_matches);
    if (total_cert_matches == 1) {
        *match_found = 1;
        *matching_cert = save_match;
    }

    retval = 0;

    pkiDebug("%s: returning %d, match_found %d\n",
             __FUNCTION__, retval, *match_found);
    return retval;
}

static krb5_error_code
free_all_cert_matching_data(krb5_context context,
                            pkinit_cert_matching_data **matchdata)
{
    krb5_error_code retval;
    pkinit_cert_matching_data *md;
    int i;

    if (matchdata == NULL)
        return EINVAL;

    for (i = 0, md = matchdata[i]; md != NULL; md = matchdata[++i]) {
        pkinit_cert_handle ch = md->ch;
        retval = crypto_cert_free_matching_data(context, md);
        if (retval) {
            pkiDebug("%s: crypto_cert_free_matching_data error %d, %s\n",
                     __FUNCTION__, retval, error_message(retval));
            goto cleanup;
        }
        retval = crypto_cert_release(context, ch);
        if (retval) {
            pkiDebug("%s: crypto_cert_release error %d, %s\n",
                     __FUNCTION__, retval, error_message(retval));
            goto cleanup;
        }
    }
    free(matchdata);
    retval = 0;

cleanup:
    return retval;
}

static krb5_error_code
obtain_all_cert_matching_data(krb5_context context,
                              pkinit_plg_crypto_context plg_cryptoctx,
                              pkinit_req_crypto_context req_cryptoctx,
                              pkinit_identity_crypto_context id_cryptoctx,
                              pkinit_cert_matching_data ***all_matching_data)
{
    krb5_error_code retval;
    int i, cert_count;
    pkinit_cert_iter_handle ih = NULL;
    pkinit_cert_handle ch;
    pkinit_cert_matching_data **matchdata = NULL;

    retval = crypto_cert_get_count(context, plg_cryptoctx, req_cryptoctx,
                                   id_cryptoctx, &cert_count);
    if (retval) {
        pkiDebug("%s: crypto_cert_get_count error %d, %s\n",
                 __FUNCTION__, retval, error_message(retval));
        goto cleanup;
    }

    pkiDebug("%s: crypto_cert_get_count says there are %d certs\n",
             __FUNCTION__, cert_count);

    matchdata = calloc((size_t)cert_count + 1, sizeof(*matchdata));
    if (matchdata == NULL)
        return ENOMEM;

    retval = crypto_cert_iteration_begin(context, plg_cryptoctx, req_cryptoctx,
                                         id_cryptoctx, &ih);
    if (retval) {
        pkiDebug("%s: crypto_cert_iteration_begin returned %d, %s\n",
                 __FUNCTION__, retval, error_message(retval));
        goto cleanup;
    }

    for (i = 0; i < cert_count; i++) {
        retval = crypto_cert_iteration_next(context, ih, &ch);
        if (retval) {
            if (retval == PKINIT_ITER_NO_MORE)
                pkiDebug("%s: We thought there were %d certs, but "
                         "crypto_cert_iteration_next stopped after %d?\n",
                         __FUNCTION__, cert_count, i);
            else
                pkiDebug("%s: crypto_cert_iteration_next error %d, %s\n",
                         __FUNCTION__, retval, error_message(retval));
            goto cleanup;
        }

        retval = crypto_cert_get_matching_data(context, ch, &matchdata[i]);
        if (retval) {
            pkiDebug("%s: crypto_cert_get_matching_data error %d, %s\n",
                     __FUNCTION__, retval, error_message(retval));
            goto cleanup;
        }

    }

    *all_matching_data = matchdata;
    retval = 0;
cleanup:
    if (ih != NULL)
        crypto_cert_iteration_end(context, ih);
    if (retval) {
        if (matchdata != NULL)
            free_all_cert_matching_data(context, matchdata);
    }
    pkiDebug("%s: returning %d, certinfo %p\n",
             __FUNCTION__, retval, *all_matching_data);
    return retval;
}

krb5_error_code
pkinit_cert_matching(krb5_context context,
                     pkinit_plg_crypto_context plg_cryptoctx,
                     pkinit_req_crypto_context req_cryptoctx,
                     pkinit_identity_crypto_context id_cryptoctx,
                     krb5_principal princ)
{

    krb5_error_code retval = KRB5KDC_ERR_PREAUTH_FAILED;
    int x;
    char **rules = NULL;
    rule_set *rs = NULL;
    int match_found = 0;
    pkinit_cert_matching_data **matchdata = NULL;
    pkinit_cert_matching_data *the_matching_cert = NULL;

    /* If no matching rules, select the default cert and we're done */
    pkinit_libdefault_strings(context, krb5_princ_realm(context, princ),
                              KRB5_CONF_PKINIT_CERT_MATCH, &rules);
    if (rules == NULL) {
        pkiDebug("%s: no matching rules found in config file\n", __FUNCTION__);
        retval = crypto_cert_select_default(context, plg_cryptoctx,
                                            req_cryptoctx, id_cryptoctx);
        goto cleanup;
    }

    /* parse each rule line one at a time and check all the certs against it */
    for (x = 0; rules[x] != NULL; x++) {
        pkiDebug("%s: Processing rule '%s'\n", __FUNCTION__, rules[x]);

        /* Free rules from previous time through... */
        if (rs != NULL) {
            free_rule_set(context, rs);
            rs = NULL;
        }
        retval = parse_rule_set(context, rules[x], &rs);
        if (retval) {
            if (retval == EINVAL) {
                pkiDebug("%s: Ignoring invalid rule pkinit_cert_match = '%s'\n",
                         __FUNCTION__, rules[x]);
                continue;
            }
            goto cleanup;
        }

        /*
         * Optimize so that we do not get cert info unless we have
         * valid rules to check.  Once obtained, keep it around
         * until we are done.
         */
        if (matchdata == NULL) {
            retval = obtain_all_cert_matching_data(context, plg_cryptoctx,
                                                   req_cryptoctx, id_cryptoctx,
                                                   &matchdata);
            if (retval || matchdata == NULL) {
                pkiDebug("%s: Error %d obtaining certificate information\n",
                         __FUNCTION__, retval);
                retval = ENOENT;
                goto cleanup;
            }
        }

        retval = check_all_certs(context, plg_cryptoctx, req_cryptoctx,
                                 id_cryptoctx, princ, rs, matchdata,
                                 &match_found, &the_matching_cert);
        if (retval) {
            pkiDebug("%s: Error %d, checking certs against rule '%s'\n",
                     __FUNCTION__, retval, rules[x]);
            goto cleanup;
        }
        if (match_found) {
            pkiDebug("%s: We have an exact match with rule '%s'\n",
                     __FUNCTION__, rules[x]);
            break;
        }
    }

    if (match_found && the_matching_cert != NULL) {
        pkiDebug("%s: Selecting the matching cert!\n", __FUNCTION__);
        retval = crypto_cert_select(context, the_matching_cert);
        if (retval) {
            pkiDebug("%s: crypto_cert_select error %d, %s\n",
                     __FUNCTION__, retval, error_message(retval));
            goto cleanup;
        }
    } else {
        retval = ENOENT;    /* XXX */
        goto cleanup;
    }

    retval = 0;
cleanup:
    if (rules != NULL)
        profile_free_list(rules);
    if (rs != NULL)
        free_rule_set(context, rs);
    if (matchdata != NULL)
        free_all_cert_matching_data(context, matchdata);
    return retval;
}
