/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/os/localauth_rule.c - rule localauth module */
/*
 * Copyright (C) 1990,1991,2007,2008,2013 by the Massachusetts
 * Institute of Technology.  All rights reserved.
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
 * This module implements the RULE type for auth_to_local processing.
 *
 * There are three parts to each rule.  The first part, if present, determines
 * the selection string.  If this is not present, the selection string defaults
 * to the unparsed principal name without realm (which can be dangerous in
 * multi-realm environments, but is our historical behavior).  The selection
 * string syntax is:
 *
 *     "[" <ncomps> ":" <format> "]"
 *
 *         <ncomps> is the number of expected components for this rule.  If the
 *         principal does not have this many components, then this rule does
 *         not apply.
 *
 *         <format> determines the selection string.  Within <format>, $0 will
 *         be substituted with the principal's realm, $1 with its first
 *         component, $2 with its second component, and so forth.
 *
 * The second part is an optional regular expression surrounded by parentheses.
 * If present, the rule will only apply if the selection string matches the
 * regular expression.  At present, the regular expression may not contain a
 * ')' character.
 *
 * The third part is a sequence of zero or more transformation rules, using
 * the syntax:
 *
 *     "s/" <regexp> "/" <text> "/" ["g"]
 *
 * No substitutions are allowed within <text>.  A "g" indicates that the
 * substitution should be performed globally; otherwise it will be performed at
 * most once.
 */

#include "k5-int.h"
#include "os-proto.h"
#include <krb5/localauth_plugin.h>
#include <ctype.h>

#ifdef HAVE_REGEX_H
#include <regex.h>

/* Process the match portion of a rule and update *contextp.  Return
 * KRB5_LNAME_NOTRANS if selstring doesn't match the regexp. */
static krb5_error_code
aname_do_match(const char *selstring, const char **contextp)
{
    krb5_error_code ret;
    const char *startp, *endp;
    char *regstr;
    regex_t re;
    regmatch_t m;

    /* If no regexp is present, leave *contextp alone and return success. */
    if (**contextp != '(')
        return 0;

    /* Find the end of the regexp and make a copy of it. */
    startp = *contextp + 1;
    endp = strchr(startp, ')');
    if (endp == NULL)
        return KRB5_CONFIG_BADFORMAT;
    regstr = k5memdup0(startp, endp - startp, &ret);
    if (regstr == NULL)
        return ret;

    /* Perform the match. */
    ret = (regcomp(&re, regstr, REG_EXTENDED) == 0 &&
           regexec(&re, selstring, 1, &m, 0) == 0 &&
           m.rm_so == 0 && (size_t)m.rm_eo == strlen(selstring)) ? 0 :
        KRB5_LNAME_NOTRANS;
    regfree(&re);
    free(regstr);
    *contextp = endp + 1;
    return ret;
}

/* Replace regular expression matches of regstr with repl in instr, producing
 * *outstr.  If doall is true, replace all matches for regstr. */
static krb5_error_code
do_replacement(const char *regstr, const char *repl, krb5_boolean doall,
               const char *instr, char **outstr)
{
    struct k5buf buf;
    regex_t re;
    regmatch_t m;

    *outstr = NULL;
    if (regcomp(&re, regstr, REG_EXTENDED))
        return KRB5_LNAME_NOTRANS;
    k5_buf_init_dynamic(&buf);
    while (regexec(&re, instr, 1, &m, 0) == 0) {
        k5_buf_add_len(&buf, instr, m.rm_so);
        k5_buf_add(&buf, repl);
        instr += m.rm_eo;
        if (!doall)
            break;
    }
    regfree(&re);
    k5_buf_add(&buf, instr);
    if (k5_buf_status(&buf) != 0)
        return ENOMEM;
    *outstr = buf.data;
    return 0;
}

/*
 * Perform any substitutions specified by *contextp, and advance *contextp past
 * the substitution expressions.  Place the result of the substitutions in
 * *result.
 */
static krb5_error_code
aname_replacer(const char *string, const char **contextp, char **result)
{
    krb5_error_code ret = 0;
    const char *cp, *ep, *tp;
    char *current, *newstr, *rule = NULL, *repl = NULL;
    krb5_boolean doglobal;

    *result = NULL;

    current = strdup(string);
    if (current == NULL)
        return ENOMEM;

    /* Iterate over replacement expressions, updating current for each one. */
    cp = *contextp;
    while (*cp != '\0') {
        /* Skip leading whitespace */
        while (isspace((unsigned char)*cp))
            cp++;

        /* Find the separators for an s/rule/repl/ expression. */
        if (!(cp[0] == 's' && cp[1] == '/' && (ep = strchr(cp + 2, '/')) &&
              (tp = strchr(ep + 1, '/')))) {
            ret = KRB5_CONFIG_BADFORMAT;
            goto cleanup;
        }

        /* Copy the rule and replacement strings. */
        free(rule);
        rule = k5memdup0(cp + 2, ep - (cp + 2), &ret);
        if (rule == NULL)
            goto cleanup;
        free(repl);
        repl = k5memdup0(ep + 1, tp - (ep + 1), &ret);
        if (repl == NULL)
            goto cleanup;

        /* Advance past expression and check for trailing "g". */
        cp = tp + 1;
        doglobal = (*cp == 'g');
        if (doglobal)
            cp++;

        ret = do_replacement(rule, repl, doglobal, current, &newstr);
        if (ret)
            goto cleanup;
        free(current);
        current = newstr;
    }
    *result = current;

cleanup:
    free(repl);
    free(rule);
    return ret;
}

/*
 * Compute selection string for RULE rules.  Advance *contextp to the string
 * position after the selstring part if present, and set *result to the
 * selection string.
 */
static krb5_error_code
aname_get_selstring(krb5_context context, krb5_const_principal aname,
                    const char **contextp, char **selstring_out)
{
    const char *current;
    char *end;
    long num_comps, ind;
    const krb5_data *datap;
    struct k5buf selstring;
    size_t nlit;

    *selstring_out = NULL;
    if (**contextp != '[') {
        /*
         * No selstring part; use the principal name without realm.  This is
         * problematic in many multiple-realm environments, but is how we've
         * historically done it.
         */
        return krb5_unparse_name_flags(context, aname,
                                       KRB5_PRINCIPAL_UNPARSE_NO_REALM,
                                       selstring_out);
    }

    /* Advance past the '[' and read the number of components. */
    current = *contextp + 1;
    errno = 0;
    num_comps = strtol(current, &end, 10);
    if (errno != 0 || num_comps < 0 || *end != ':')
        return KRB5_CONFIG_BADFORMAT;
    current = end;
    if (num_comps != aname->length)
        return KRB5_LNAME_NOTRANS;
    current++;

    k5_buf_init_dynamic(&selstring);
    while (TRUE) {
        /* Copy in literal characters up to the next $ or ]. */
        nlit = strcspn(current, "$]");
        k5_buf_add_len(&selstring, current, nlit);
        current += nlit;
        if (*current != '$')
            break;

        /* Expand $ substitution to a principal component. */
        errno = 0;
        ind = strtol(current + 1, &end, 10);
        if (errno || ind > num_comps)
            break;
        current = end;
        datap = ind > 0 ? &aname->data[ind - 1] : &aname->realm;
        k5_buf_add_len(&selstring, datap->data, datap->length);
    }

    /* Check that we hit a ']' and not the end of the string. */
    if (*current != ']') {
        k5_buf_free(&selstring);
        return KRB5_CONFIG_BADFORMAT;
    }

    if (k5_buf_status(&selstring) != 0)
        return ENOMEM;

    *contextp = current + 1;
    *selstring_out = selstring.data;
    return 0;
}

static krb5_error_code
an2ln_rule(krb5_context context, krb5_localauth_moddata data, const char *type,
           const char *rule, krb5_const_principal aname, char **lname_out)
{
    krb5_error_code ret;
    const char *current;
    char *selstring = NULL;

    *lname_out = NULL;
    if (rule == NULL)
        return KRB5_CONFIG_BADFORMAT;

    /* Compute the selection string. */
    current = rule;
    ret = aname_get_selstring(context, aname, &current, &selstring);
    if (ret)
        return ret;

    /* Check the selection string against the regexp, if present. */
    if (*current == '(') {
        ret = aname_do_match(selstring, &current);
        if (ret)
            goto cleanup;
    }

    /* Perform the substitution. */
    ret = aname_replacer(selstring, &current, lname_out);

cleanup:
    free(selstring);
    return ret;
}

#else /* HAVE_REGEX_H */

static krb5_error_code
an2ln_rule(krb5_context context, krb5_localauth_moddata data, const char *type,
           const char *rule, krb5_const_principal aname, char **lname_out)
{
    return KRB5_LNAME_NOTRANS;
}

#endif

static void
freestr(krb5_context context, krb5_localauth_moddata data, char *str)
{
    free(str);
}

krb5_error_code
localauth_rule_initvt(krb5_context context, int maj_ver, int min_ver,
                      krb5_plugin_vtable vtable)
{
    krb5_localauth_vtable vt = (krb5_localauth_vtable)vtable;
    static const char *types[] = { "RULE", NULL };

    vt->name = "rule";
    vt->an2ln_types = types;
    vt->an2ln = an2ln_rule;
    vt->free_string = freestr;
    return 0;
}
