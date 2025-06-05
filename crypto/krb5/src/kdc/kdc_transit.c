/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* kdc/kdc_transit.c */
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

#include "k5-int.h"
#include "kdc_util.h"

#define MAX_REALM_LN 500

/*
 * subrealm - determine if r2 is a subrealm of r1
 *
 *            SUBREALM takes two realms, r1 and r2, and
 *            determines if r2 is a subrealm of r1.
 *            r2 is a subrealm of r1 if (r1 is a prefix
 *            of r2 AND r1 and r2 begin with a /) or if
 *            (r1 is a suffix of r2 and neither r1 nor r2
 *            begin with a /).
 *
 * RETURNS:   If r2 is a subrealm, and r1 is a prefix, the number
 *            of characters in the suffix of r2 is returned as a
 *            negative number.
 *
 *            If r2 is a subrealm, and r1 is a suffix, the number
 *            of characters in the prefix of r2 is returned as a
 *            positive number.
 *
 *            If r2 is not a subrealm, SUBREALM returns 0.
 */
static  int
subrealm(char *r1, char *r2)
{
    size_t l1,l2;
    l1 = strlen(r1);
    l2 = strlen(r2);
    if(l2 <= l1) return(0);
    if((*r1 == '/') && (*r2 == '/') && (strncmp(r1,r2,l1) == 0)) return(l1-l2);
    if((*r1 != '/') && (*r2 != '/') && (strncmp(r1,r2+l2-l1,l1) == 0))
        return(l2-l1);
    return(0);
}

/*
 * add_to_transited  Adds the name of the realm which issued the
 *                   ticket granting ticket on which the new ticket to
 *                   be issued is based (note that this is the same as
 *                   the realm of the server listed in the ticket
 *                   granting ticket.
 *
 * ASSUMPTIONS:  This procedure assumes that the transited field from
 *               the existing ticket granting ticket already appears
 *               in compressed form.  It will add the new realm while
 *               maintaining that form.   As long as each successive
 *               realm is added using this (or a similar) routine, the
 *               transited field will be in compressed form.  The
 *               basis step is an empty transited field which is, by
 *               its nature, in its most compressed form.
 *
 * ARGUMENTS: krb5_data *tgt_trans  Transited field from TGT
 *            krb5_data *new_trans  The transited field for the new ticket
 *            krb5_principal tgs    Name of ticket granting server
 *                                  This includes the realm of the KDC
 *                                  that issued the ticket granting
 *                                  ticket.  This is the realm that is
 *                                  to be added to the transited field.
 *            krb5_principal client Name of the client
 *            krb5_principal server The name of the requested server.
 *                                  This may be the an intermediate
 *                                  ticket granting server.
 *
 *            The last two argument are needed since they are
 *            implicitly part of the transited field of the new ticket
 *            even though they are not explicitly listed.
 *
 * RETURNS:   krb5_error_code - Success, or out of memory
 *
 * MODIFIES:  new_trans:  ->length will contain the length of the new
 *                        transited field.
 *
 *                        If ->data was not null when this procedure
 *                        is called, the memory referenced by ->data
 *                        will be deallocated.
 *
 *                        Memory will be allocated for the new transited field
 *                        ->data will be updated to point to the newly
 *                        allocated memory.
 *
 * BUGS:  The space allocated for the new transited field is the
 *        maximum that might be needed given the old transited field,
 *        and the realm to be added.  This length is calculated
 *        assuming that no compression of the new realm is possible.
 *        This has no adverse consequences other than the allocation
 *        of more space than required.
 *
 *        This procedure will not yet use the null subfield notation,
 *        and it will get confused if it sees it.
 *
 *        This procedure does not check for quoted commas in realm
 *        names.
 */

char *
data2string (krb5_data *d)
{
    char *s;
    s = malloc(d->length + 1);
    if (s) {
        if (d->length > 0)
            memcpy(s, d->data, d->length);
        s[d->length] = 0;
    }
    return s;
}

krb5_error_code
add_to_transited(krb5_data *tgt_trans, krb5_data *new_trans,
                 krb5_principal tgs, krb5_principal client,
                 krb5_principal server)
{
    krb5_error_code retval;
    char        *realm;
    char        *trans;
    char        *otrans, *otrans_ptr;
    size_t       bufsize;

    /* The following are for stepping through the transited field     */

    char        prev[MAX_REALM_LN];
    char        next[MAX_REALM_LN];
    char        current[MAX_REALM_LN];
    char        exp[MAX_REALM_LN];      /* Expanded current realm name     */

    int         i;
    int         clst, nlst;    /* count of last character in current and next */
    int         pl, pl1;       /* prefix length                               */
    int         added;         /* TRUE = new realm has been added             */

    realm = data2string(krb5_princ_realm(kdc_context, tgs));
    if (realm == NULL)
        return(ENOMEM);

    otrans = data2string(tgt_trans);
    if (otrans == NULL) {
        free(realm);
        return(ENOMEM);
    }
    /* Keep track of start so we can free */
    otrans_ptr = otrans;

    /* +1 for null,
       +1 for extra comma which may be added between
       +1 for potential space when leading slash in realm */
    bufsize = strlen(realm) + strlen(otrans) + 3;
    if (bufsize > MAX_REALM_LN)
        bufsize = MAX_REALM_LN;
    if (!(trans = (char *) malloc(bufsize))) {
        retval = ENOMEM;
        goto fail;
    }

    if (new_trans->data)  free(new_trans->data);
    new_trans->data = trans;
    new_trans->length = 0;

    trans[0] = '\0';

    /* For the purpose of appending, the realm preceding the first */
    /* realm in the transited field is considered the null realm   */

    prev[0] = '\0';

    /* read field into current */
    for (i = 0; *otrans != '\0';) {
        if (*otrans == '\\') {
            if (*(++otrans) == '\0')
                break;
            else
                continue;
        }
        if (*otrans == ',') {
            otrans++;
            break;
        }
        current[i++] = *otrans++;
        if (i >= MAX_REALM_LN) {
            retval = KRB5KRB_AP_ERR_ILL_CR_TKT;
            goto fail;
        }
    }
    current[i] = '\0';

    added = (krb5_princ_realm(kdc_context, client)->length == strlen(realm) &&
             !strncmp(krb5_princ_realm(kdc_context, client)->data, realm, strlen(realm))) ||
        (krb5_princ_realm(kdc_context, server)->length == strlen(realm) &&
         !strncmp(krb5_princ_realm(kdc_context, server)->data, realm, strlen(realm)));

    while (current[0]) {

        /* figure out expanded form of current name */

        clst = strlen(current) - 1;
        if (current[0] == ' ') {
            strncpy(exp, current+1, sizeof(exp) - 1);
            exp[sizeof(exp) - 1] = '\0';
        }
        else if ((current[0] == '/') && (prev[0] == '/')) {
            strncpy(exp, prev, sizeof(exp) - 1);
            exp[sizeof(exp) - 1] = '\0';
            if (strlen(exp) + strlen(current) + 1 >= MAX_REALM_LN) {
                retval = KRB5KRB_AP_ERR_ILL_CR_TKT;
                goto fail;
            }
            strncat(exp, current, sizeof(exp) - 1 - strlen(exp));
        }
        else if (current[clst] == '.') {
            strncpy(exp, current, sizeof(exp) - 1);
            exp[sizeof(exp) - 1] = '\0';
            if (strlen(exp) + strlen(prev) + 1 >= MAX_REALM_LN) {
                retval = KRB5KRB_AP_ERR_ILL_CR_TKT;
                goto fail;
            }
            strncat(exp, prev, sizeof(exp) - 1 - strlen(exp));
        }
        else {
            strncpy(exp, current, sizeof(exp) - 1);
            exp[sizeof(exp) - 1] = '\0';
        }

        /* read field into next */
        for (i = 0; *otrans != '\0';) {
            if (*otrans == '\\') {
                if (*(++otrans) == '\0')
                    break;
                else
                    continue;
            }
            if (*otrans == ',') {
                otrans++;
                break;
            }
            next[i++] = *otrans++;
            if (i >= MAX_REALM_LN) {
                retval = KRB5KRB_AP_ERR_ILL_CR_TKT;
                goto fail;
            }
        }
        next[i] = '\0';
        nlst = i - 1;

        if (!strcmp(exp, realm))  added = TRUE;

        /* If we still have to insert the new realm */

        if (!added) {

            /* Is the next field compressed?  If not, and if the new */
            /* realm is a subrealm of the current realm, compress    */
            /* the new realm, and insert immediately following the   */
            /* current one.  Note that we can not do this if the next*/
            /* field is already compressed since it would mess up    */
            /* what has already been done.  In most cases, this is   */
            /* not a problem because the realm to be added will be a */
            /* subrealm of the next field too, and we will catch     */
            /* it in a future iteration.                             */

            /* Note that the second test here is an unsigned comparison,
               so the first half (or a cast) is also required.  */
            assert(nlst < 0 || nlst < (int)sizeof(next));
            if ((nlst < 0 || next[nlst] != '.') &&
                (next[0] != '/') &&
                (pl = subrealm(exp, realm))) {
                added = TRUE;
                current[sizeof(current) - 1] = '\0';
                if (strlen(current) + (pl>0?pl:-pl) + 2 >= MAX_REALM_LN) {
                    retval = KRB5KRB_AP_ERR_ILL_CR_TKT;
                    goto fail;
                }
                strncat(current, ",", sizeof(current) - 1 - strlen(current));
                if (pl > 0) {
                    strncat(current, realm, (unsigned) pl);
                }
                else {
                    strncat(current, realm+strlen(realm)+pl, (unsigned) (-pl));
                }
            }

            /* Whether or not the next field is compressed, if the    */
            /* realm to be added is a superrealm of the current realm,*/
            /* then the current realm can be compressed.  First the   */
            /* realm to be added must be compressed relative to the   */
            /* previous realm (if possible), and then the current     */
            /* realm compressed relative to the new realm.  Note that */
            /* if the realm to be added is also a superrealm of the   */
            /* previous realm, it would have been added earlier, and  */
            /* we would not reach this step this time around.         */

            else if ((pl = subrealm(realm, exp))) {
                added      = TRUE;
                current[0] = '\0';
                if ((pl1 = subrealm(prev,realm))) {
                    if (strlen(current) + (pl1>0?pl1:-pl1) + 1 >= MAX_REALM_LN) {
                        retval = KRB5KRB_AP_ERR_ILL_CR_TKT;
                        goto fail;
                    }
                    if (pl1 > 0) {
                        strncat(current, realm, (unsigned) pl1);
                    }
                    else {
                        strncat(current, realm+strlen(realm)+pl1, (unsigned) (-pl1));
                    }
                }
                else { /* If not a subrealm */
                    if ((realm[0] == '/') && prev[0]) {
                        if (strlen(current) + 2 >= MAX_REALM_LN) {
                            retval = KRB5KRB_AP_ERR_ILL_CR_TKT;
                            goto fail;
                        }
                        strncat(current, " ", sizeof(current) - 1 - strlen(current));
                        current[sizeof(current) - 1] = '\0';
                    }
                    if (strlen(current) + strlen(realm) + 1 >= MAX_REALM_LN) {
                        retval = KRB5KRB_AP_ERR_ILL_CR_TKT;
                        goto fail;
                    }
                    strncat(current, realm, sizeof(current) - 1 - strlen(current));
                    current[sizeof(current) - 1] = '\0';
                }
                if (strlen(current) + (pl>0?pl:-pl) + 2 >= MAX_REALM_LN) {
                    retval = KRB5KRB_AP_ERR_ILL_CR_TKT;
                    goto fail;
                }
                strncat(current,",", sizeof(current) - 1 - strlen(current));
                current[sizeof(current) - 1] = '\0';
                if (pl > 0) {
                    strncat(current, exp, (unsigned) pl);
                }
                else {
                    strncat(current, exp+strlen(exp)+pl, (unsigned)(-pl));
                }
            }
        }

        if (new_trans->length != 0) {
            if (strlcat(trans, ",", bufsize) >= bufsize) {
                retval = KRB5KRB_AP_ERR_ILL_CR_TKT;
                goto fail;
            }
        }
        if (strlcat(trans, current, bufsize) >= bufsize) {
            retval = KRB5KRB_AP_ERR_ILL_CR_TKT;
            goto fail;
        }
        new_trans->length = strlen(trans);

        strncpy(prev, exp, sizeof(prev) - 1);
        prev[sizeof(prev) - 1] = '\0';
        strncpy(current, next, sizeof(current) - 1);
        current[sizeof(current) - 1] = '\0';
    }

    if (!added) {
        if (new_trans->length != 0) {
            if (strlcat(trans, ",", bufsize) >= bufsize) {
                retval = KRB5KRB_AP_ERR_ILL_CR_TKT;
                goto fail;
            }
        }
        if((realm[0] == '/') && trans[0]) {
            if (strlcat(trans, " ", bufsize) >= bufsize) {
                retval = KRB5KRB_AP_ERR_ILL_CR_TKT;
                goto fail;
            }
        }
        if (strlcat(trans, realm, bufsize) >= bufsize) {
            retval = KRB5KRB_AP_ERR_ILL_CR_TKT;
            goto fail;
        }
        new_trans->length = strlen(trans);
    }

    retval = 0;
fail:
    free(realm);
    free(otrans_ptr);
    return (retval);
}
