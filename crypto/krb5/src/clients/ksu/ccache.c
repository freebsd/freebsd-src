/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright (c) 1994 by the University of Southern California
 *
 * EXPORT OF THIS SOFTWARE from the United States of America may
 *     require a specific license from the United States Government.
 *     It is the responsibility of any person or organization contemplating
 *     export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to copy, modify, and distribute
 *     this software and its documentation in source and binary forms is
 *     hereby granted, provided that any documentation or other materials
 *     related to such distribution or use acknowledge that the software
 *     was developed by the University of Southern California.
 *
 * DISCLAIMER OF WARRANTY.  THIS SOFTWARE IS PROVIDED "AS IS".  The
 *     University of Southern California MAKES NO REPRESENTATIONS OR
 *     WARRANTIES, EXPRESS OR IMPLIED.  By way of example, but not
 *     limitation, the University of Southern California MAKES NO
 *     REPRESENTATIONS OR WARRANTIES OF MERCHANTABILITY OR FITNESS FOR ANY
 *     PARTICULAR PURPOSE. The University of Southern
 *     California shall not be held liable for any liability nor for any
 *     direct, indirect, or consequential damages with respect to any
 *     claim by the user or distributor of the ksu software.
 *
 * KSU was written by:  Ari Medvinsky, ari@isi.edu
 */

#include "ksu.h"
#include "k5-base64.h"
#include "adm_proto.h"
#include <sys/types.h>
#include <sys/stat.h>

/******************************************************************
krb5_cache_copy

gets rid of any expired tickets in the secondary cache,
copies the default cache into the secondary cache,

************************************************************************/

static void
free_creds_list(krb5_context context, krb5_creds **list)
{
    size_t i;

    if (list == NULL)
        return;
    for (i = 0; list[i]; i++)
        krb5_free_creds(context, list[i]);
    free(list);
}

void show_credential(krb5_context, krb5_creds *, krb5_ccache);

/* modifies only the cc_other, the algorithm may look a bit funny,
   but I had to do it this way, since remove function did not come
   with k5 beta 3 release.
*/

krb5_error_code
krb5_ccache_copy(krb5_context context, krb5_ccache cc_def,
                 krb5_principal target_principal, krb5_ccache cc_target,
                 krb5_boolean restrict_creds, krb5_principal primary_principal,
                 krb5_boolean *stored)
{
    krb5_error_code retval=0;
    krb5_creds ** cc_def_creds_arr = NULL;
    krb5_creds ** cc_other_creds_arr = NULL;

    if (ks_ccache_is_initialized(context, cc_def)) {
        retval = krb5_get_nonexp_tkts(context, cc_def, &cc_def_creds_arr);
        if (retval)
            goto cleanup;
    }

    retval = krb5_cc_initialize(context, cc_target, target_principal);
    if (retval)
        goto cleanup;

    if (restrict_creds) {
        retval = krb5_store_some_creds(context, cc_target, cc_def_creds_arr,
                                       cc_other_creds_arr, primary_principal,
                                       stored);
    } else {
        *stored = krb5_find_princ_in_cred_list(context, cc_def_creds_arr,
                                               primary_principal);
        retval = krb5_store_all_creds(context, cc_target, cc_def_creds_arr,
                                      cc_other_creds_arr);
    }

cleanup:
    free_creds_list(context, cc_def_creds_arr);
    free_creds_list(context, cc_other_creds_arr);
    return retval;
}


krb5_error_code
krb5_store_all_creds(krb5_context context, krb5_ccache cc,
                     krb5_creds **creds_def, krb5_creds **creds_other)
{

    int i = 0;
    krb5_error_code retval = 0;
    krb5_creds ** temp_creds= NULL;


    if ((creds_def == NULL) && (creds_other == NULL))
        return 0;

    if ((creds_def == NULL) && (creds_other != NULL))
        temp_creds = creds_other;

    if ((creds_def != NULL) && (creds_other == NULL))
        temp_creds = creds_def;


    if (temp_creds){
        while(temp_creds[i]){
            if ((retval= krb5_cc_store_cred(context, cc,
                                            temp_creds[i]))){
                return retval;
            }
            i++;
        }
    }
    else { /* both arrays have elements in them */

        return  KRB5KRB_ERR_GENERIC;

/************   while(creds_other[i]){
                        cmp = FALSE;
                        j = 0;
                        while(creds_def[j]){
                           cmp = compare_creds(creds_other[i],creds_def[j]);

                           if( cmp == TRUE) break;

                           j++;
                        }
                        if (cmp == FALSE){
                                if (retval= krb5_cc_store_cred(context, cc,
                                                         creds_other[i])){
                                                return retval;
                                }
                        }
                        i ++;
                }

                i=0;
                while(creds_def[i]){
                        if (retval= krb5_cc_store_cred(context, cc,
                                                       creds_def[i])){
                                return retval;
                        }
                        i++;
                }

**************/
    }
    return 0;
}

krb5_boolean
compare_creds(krb5_context context, krb5_creds *cred1, krb5_creds *cred2)
{
    krb5_boolean retval;

    retval = krb5_principal_compare (context, cred1->client, cred2->client);

    if (retval == TRUE)
        retval = krb5_principal_compare (context, cred1->server,                                                         cred2->server);

    return retval;
}

krb5_error_code
krb5_get_nonexp_tkts(krb5_context context, krb5_ccache cc,
                     krb5_creds ***creds_array)
{

    krb5_creds creds, temp_tktq, temp_tkt;
    krb5_creds **temp_creds = NULL;
    krb5_error_code retval=0;
    krb5_cc_cursor cur;
    int count = 0;
    int chunk_count = 1;

    temp_creds = xcalloc(CHUNK, sizeof(*temp_creds));
    memset(&temp_tktq, 0, sizeof(temp_tktq));
    memset(&temp_tkt, 0, sizeof(temp_tkt));
    memset(&creds, 0, sizeof(creds));

    /* initialize the cursor */
    retval = krb5_cc_start_seq_get(context, cc, &cur);
    if (retval)
        goto cleanup;

    while (!(retval = krb5_cc_next_cred(context, cc, &cur, &creds))){

        if (!krb5_is_config_principal(context, creds.server) &&
            (retval = krb5_check_exp(context, creds.times))){
            krb5_free_cred_contents(context, &creds);
            if (retval != KRB5KRB_AP_ERR_TKT_EXPIRED){
                goto cleanup;
            }
            if (auth_debug){
                fprintf(stderr,"krb5_ccache_copy: CREDS EXPIRED:\n");
                fputs("  Valid starting         Expires         Service principal\n",stdout);
                show_credential(context, &creds, cc);
                fprintf(stderr,"\n");
            }
        }
        else {   /* these credentials didn't expire */
            retval = krb5_copy_creds(context, &creds, &temp_creds[count]);
            krb5_free_cred_contents(context, &creds);
            temp_creds[count+1] = NULL;
            if (retval)
                goto cleanup;
            count ++;

            if (count == (chunk_count * CHUNK -1)){
                chunk_count ++;

                temp_creds = xrealloc(temp_creds,
                                      chunk_count * CHUNK *
                                      sizeof(*temp_creds));
            }
        }

    }

    temp_creds[count] = NULL;
    *creds_array   = temp_creds;
    temp_creds = NULL;

    if (retval == KRB5_CC_END) {
        retval = krb5_cc_end_seq_get(context, cc, &cur);
    }

cleanup:
    free_creds_list(context, temp_creds);
    return retval;
}

krb5_error_code
krb5_check_exp(krb5_context context, krb5_ticket_times tkt_time)
{
    krb5_error_code retval =0;
    krb5_timestamp currenttime;

    if ((retval = krb5_timeofday (context, &currenttime))){
        return retval;
    }
    if (auth_debug){
        fprintf(stderr,"krb5_check_exp: the krb5_clockskew is %d \n",
                context->clockskew);

        fprintf(stderr,"krb5_check_exp: currenttime - endtime %d \n",
                ts_delta(currenttime, tkt_time.endtime));

    }

    if (ts_after(currenttime, ts_incr(tkt_time.endtime, context->clockskew))) {
        retval = KRB5KRB_AP_ERR_TKT_EXPIRED ;
        return retval;
    }

    return 0;
}

char *
flags_string(krb5_creds *cred)
{
    static char buf[32];
    int i = 0;

    if (cred->ticket_flags & TKT_FLG_FORWARDABLE)
        buf[i++] = 'F';
    if (cred->ticket_flags & TKT_FLG_FORWARDED)
        buf[i++] = 'f';
    if (cred->ticket_flags & TKT_FLG_PROXIABLE)
        buf[i++] = 'P';
    if (cred->ticket_flags & TKT_FLG_PROXY)
        buf[i++] = 'p';
    if (cred->ticket_flags & TKT_FLG_MAY_POSTDATE)
        buf[i++] = 'D';
    if (cred->ticket_flags & TKT_FLG_POSTDATED)
        buf[i++] = 'd';
    if (cred->ticket_flags & TKT_FLG_INVALID)
        buf[i++] = 'i';
    if (cred->ticket_flags & TKT_FLG_RENEWABLE)
        buf[i++] = 'R';
    if (cred->ticket_flags & TKT_FLG_INITIAL)
        buf[i++] = 'I';
    if (cred->ticket_flags & TKT_FLG_HW_AUTH)
        buf[i++] = 'H';
    if (cred->ticket_flags & TKT_FLG_PRE_AUTH)
        buf[i++] = 'A';
    buf[i] = '\0';
    return(buf);
}

void
printtime(krb5_timestamp ts)
{
    char fmtbuf[18], fill = ' ';

    if (!krb5_timestamp_to_sfstring(ts, fmtbuf, sizeof(fmtbuf), &fill))
        printf("%s", fmtbuf);
}

void
show_credential(krb5_context context, krb5_creds *cred, krb5_ccache cc)
{
    krb5_error_code retval;
    char *name = NULL, *sname = NULL, *defname = NULL, *flags;
    int first = 1;
    krb5_principal princ = NULL;
    int show_flags =1;

    retval = krb5_unparse_name(context, cred->client, &name);
    if (retval) {
        com_err(prog_name, retval, _("while unparsing client name"));
        goto cleanup;
    }
    retval = krb5_unparse_name(context, cred->server, &sname);
    if (retval) {
        com_err(prog_name, retval, _("while unparsing server name"));
        goto cleanup;
    }

    if ((retval = krb5_cc_get_principal(context, cc, &princ))) {
        com_err(prog_name, retval, _("while retrieving principal name"));
        goto cleanup;
    }
    if ((retval = krb5_unparse_name(context, princ, &defname))) {
        com_err(prog_name, retval, _("while unparsing principal name"));
        goto cleanup;
    }

    if (!cred->times.starttime)
        cred->times.starttime = cred->times.authtime;

    printtime(cred->times.starttime);
    putchar(' '); putchar(' ');
    printtime(cred->times.endtime);
    putchar(' '); putchar(' ');

    printf("%s\n", sname);

    if (strcmp(name, defname)) {
        printf(_("\tfor client %s"), name);
        first = 0;
    }

    if (cred->times.renew_till) {
        if (first)
            fputs("\t",stdout);
        else
            fputs(", ",stdout);
        fputs(_("renew until "), stdout);
        printtime(cred->times.renew_till);
    }
    if (show_flags) {
        flags = flags_string(cred);
        if (flags && *flags) {
            if (first)
                fputs("\t",stdout);
            else
                fputs(", ",stdout);
            printf(_("Flags: %s"), flags);
            first = 0;
        }
    }
    putchar('\n');

cleanup:
    free(name);
    free(sname);
    free(defname);
    krb5_free_principal(context, princ);
}

/* Create a random string suitable for a filename extension. */
krb5_error_code
gen_sym(krb5_context context, char **sym_out)
{
    krb5_error_code retval;
    char bytes[6], *p, *sym;
    krb5_data data = make_data(bytes, sizeof(bytes));

    *sym_out = NULL;
    retval = krb5_c_random_make_octets(context, &data);
    if (retval)
        return retval;
    sym = k5_base64_encode(data.data, data.length);
    if (sym == NULL)
        return ENOMEM;
    /* Tweak the output alphabet just a bit. */
    while ((p = strchr(sym, '/')) != NULL)
        *p = '_';
    while ((p = strchr(sym, '+')) != NULL)
        *p = '-';
    *sym_out = sym;
    return 0;
}

krb5_error_code
krb5_ccache_overwrite(krb5_context context, krb5_ccache ccs, krb5_ccache cct,
                      krb5_principal primary_principal)
{
    krb5_error_code retval=0;
    krb5_principal defprinc = NULL, princ;
    krb5_creds ** ccs_creds_arr = NULL;

    if (ks_ccache_is_initialized(context, ccs)) {
        retval = krb5_get_nonexp_tkts(context,  ccs, &ccs_creds_arr);
        if (retval)
            goto cleanup;
    }

    retval = krb5_cc_get_principal(context, cct, &defprinc);
    princ = (retval == 0) ? defprinc : primary_principal;
    retval = krb5_cc_initialize(context, cct, princ);
    if (retval)
        goto cleanup;

    retval = krb5_store_all_creds(context, cct, ccs_creds_arr, NULL);

cleanup:
    free_creds_list(context, ccs_creds_arr);
    krb5_free_principal(context, defprinc);
    return retval;
}

krb5_error_code
krb5_store_some_creds(krb5_context context, krb5_ccache cc,
                      krb5_creds **creds_def, krb5_creds **creds_other,
                      krb5_principal prst, krb5_boolean *stored)
{

    int i = 0;
    krb5_error_code retval = 0;
    krb5_creds ** temp_creds= NULL;
    krb5_boolean temp_stored = FALSE;


    if ((creds_def == NULL) && (creds_other == NULL))
        return 0;

    if ((creds_def == NULL) && (creds_other != NULL))
        temp_creds = creds_other;

    if ((creds_def != NULL) && (creds_other == NULL))
        temp_creds = creds_def;


    if (temp_creds){
        while(temp_creds[i]){
            if (krb5_principal_compare(context,
                                       temp_creds[i]->client,
                                       prst)== TRUE) {

                if ((retval = krb5_cc_store_cred(context,
                                                 cc,temp_creds[i]))){
                    return retval;
                }
                temp_stored = TRUE;
            }

            i++;
        }
    }
    else { /* both arrays have elements in them */
        return KRB5KRB_ERR_GENERIC;
    }

    *stored = temp_stored;
    return 0;
}

krb5_error_code
krb5_ccache_filter(krb5_context context, krb5_ccache cc, krb5_principal prst)
{

    krb5_error_code retval=0;
    krb5_principal temp_principal = NULL;
    krb5_creds ** cc_creds_arr = NULL;
    const char * cc_name;
    krb5_boolean stored;

    if (!ks_ccache_is_initialized(context, cc))
        return 0;

    if (auth_debug) {
        cc_name = krb5_cc_get_name(context, cc);
        fprintf(stderr, "putting cache %s through a filter for -z option\n",
                cc_name);
    }

    retval = krb5_get_nonexp_tkts(context, cc, &cc_creds_arr);
    if (retval)
        goto cleanup;

    retval = krb5_cc_get_principal(context, cc, &temp_principal);
    if (retval)
        goto cleanup;

    retval = krb5_cc_initialize(context, cc, temp_principal);
    if (retval)
        goto cleanup;

    retval = krb5_store_some_creds(context, cc, cc_creds_arr, NULL, prst,
                                   &stored);

cleanup:
    free_creds_list(context, cc_creds_arr);
    krb5_free_principal(context, temp_principal);
    return retval;
}

krb5_boolean
krb5_find_princ_in_cred_list(krb5_context context, krb5_creds **creds_list,
                             krb5_principal princ)
{

    int i = 0;
    krb5_boolean temp_stored = FALSE;

    if (creds_list){
        while(creds_list[i]){
            if (krb5_principal_compare(context,
                                       creds_list[i]->client,
                                       princ)== TRUE){
                temp_stored = TRUE;
                break;
            }

            i++;
        }
    }

    return temp_stored;
}

krb5_error_code
krb5_find_princ_in_cache(krb5_context context, krb5_ccache cc,
                         krb5_principal princ, krb5_boolean *found)
{
    krb5_error_code retval = 0;
    krb5_creds ** creds_list = NULL;

    if (ks_ccache_is_initialized(context, cc)) {
        retval = krb5_get_nonexp_tkts(context, cc, &creds_list);
        if (retval)
            goto cleanup;
    }

    *found = krb5_find_princ_in_cred_list(context, creds_list, princ);

cleanup:
    free_creds_list(context, creds_list);
    return retval;
}

krb5_boolean
ks_ccache_name_is_initialized(krb5_context context, const char *cctag)
{
    krb5_boolean result;
    krb5_ccache cc;

    if (krb5_cc_resolve(context, cctag, &cc) != 0)
        return FALSE;
    result = ks_ccache_is_initialized(context, cc);
    krb5_cc_close(context, cc);

    return result;
}

krb5_boolean
ks_ccache_is_initialized(krb5_context context, krb5_ccache cc)
{
    krb5_principal princ;
    krb5_error_code retval;

    if (cc == NULL)
        return FALSE;

    retval = krb5_cc_get_principal(context, cc, &princ);
    if (retval == 0)
        krb5_free_principal(context, princ);

    return retval == 0;
}
