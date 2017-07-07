/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* plugins/kdb/ldap/ldap_util/kdb5_ldap_policy.c */
/* Copyright (c) 2004-2005, Novell, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *   * The copyright holder's name is not used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Create / Delete / Modify / View / List policy objects.
 */

#include <k5-int.h>
#include <kadm5/admin.h>
#include "kdb5_ldap_util.h"
#include "kdb5_ldap_list.h"
#include "ldap_tkt_policy.h"
extern time_t get_date(char *); /* kadmin/cli/getdate.o */

static void print_policy_params(krb5_ldap_policy_params *policyparams, int mask);
static char *strdur(time_t duration);

extern char *yes;
extern kadm5_config_params global_params;

static krb5_error_code
init_ldap_realm(int argc, char *argv[])
{
    /* This operation is being performed in the context of a realm. So,
     * initialize the realm */
    int mask = 0;
    krb5_error_code retval = 0;
    kdb5_dal_handle *dal_handle = NULL;
    krb5_ldap_context *ldap_context=NULL;

    dal_handle = util_context->dal_handle;
    ldap_context = (krb5_ldap_context *) dal_handle->db_context;
    if (!ldap_context) {
        retval = EINVAL;
        goto cleanup;
    }

    if (ldap_context->container_dn == NULL) {
        retval = krb5_ldap_read_krbcontainer_dn(util_context,
                                                &ldap_context->container_dn);
        if (retval != 0) {
            com_err(progname, retval,
                    _("while reading kerberos container information"));
            goto cleanup;
        }
    }

    if (ldap_context->lrparams == NULL) {
        retval = krb5_ldap_read_realm_params(util_context,
                                             global_params.realm,
                                             &(ldap_context->lrparams),
                                             &mask);

        if (retval != 0) {
            goto cleanup;
        }
    }
cleanup:
    return retval;
}

/*
 * This function will create a ticket policy object with the
 * specified attributes.
 */
void
kdb5_ldap_create_policy(int argc, char *argv[])
{
    char *me = progname;
    krb5_error_code retval = 0;
    krb5_ldap_policy_params *policyparams = NULL;
    krb5_boolean print_usage = FALSE;
    krb5_boolean no_msg = FALSE;
    int mask = 0;
    time_t date = 0;
    time_t now = 0;
    int i = 0;

    /* Check for number of arguments */
    if ((argc < 2) || (argc > 16)) {
        goto err_usage;
    }

    /* Allocate memory for policy parameters structure */
    policyparams = (krb5_ldap_policy_params*) calloc(1, sizeof(krb5_ldap_policy_params));
    if (policyparams == NULL) {
        retval = ENOMEM;
        goto cleanup;
    }

    /* Get current time */
    time (&now);

    /* Parse all arguments */
    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-maxtktlife")) {
            if (++i > argc - 1)
                goto err_usage;

            date = get_date(argv[i]);
            if (date == (time_t)(-1)) {
                retval = EINVAL;
                com_err(me, retval, _("while providing time specification"));
                goto err_nomsg;
            }

            policyparams->maxtktlife = date - now;

            mask |= LDAP_POLICY_MAXTKTLIFE;
        } else if (!strcmp(argv[i], "-maxrenewlife")) {
            if (++i > argc - 1)
                goto err_usage;

            date = get_date(argv[i]);
            if (date == (time_t)(-1)) {
                retval = EINVAL;
                com_err(me, retval, _("while providing time specification"));
                goto err_nomsg;
            }

            policyparams->maxrenewlife = date - now;

            mask |= LDAP_POLICY_MAXRENEWLIFE;
        } else if (!strcmp((argv[i] + 1), "allow_postdated")) {
            if (*(argv[i]) == '+')
                policyparams->tktflags &= (int)(~KRB5_KDB_DISALLOW_POSTDATED);
            else if (*(argv[i]) == '-')
                policyparams->tktflags |= KRB5_KDB_DISALLOW_POSTDATED;
            else
                goto err_usage;

            mask |= LDAP_POLICY_TKTFLAGS;
        } else if (!strcmp((argv[i] + 1), "allow_forwardable")) {
            if (*(argv[i]) == '+')
                policyparams->tktflags &= (int)(~KRB5_KDB_DISALLOW_FORWARDABLE);
            else if (*(argv[i]) == '-')
                policyparams->tktflags |= KRB5_KDB_DISALLOW_FORWARDABLE;
            else
                goto err_usage;

            mask |= LDAP_POLICY_TKTFLAGS;
        } else if (!strcmp((argv[i] + 1), "allow_renewable")) {
            if (*(argv[i]) == '+')
                policyparams->tktflags &= (int)(~KRB5_KDB_DISALLOW_RENEWABLE);
            else if (*(argv[i]) == '-')
                policyparams->tktflags |= KRB5_KDB_DISALLOW_RENEWABLE;
            else
                goto err_usage;

            mask |= LDAP_POLICY_TKTFLAGS;
        } else if (!strcmp((argv[i] + 1), "allow_proxiable")) {
            if (*(argv[i]) == '+')
                policyparams->tktflags &= (int)(~KRB5_KDB_DISALLOW_PROXIABLE);
            else if (*(argv[i]) == '-')
                policyparams->tktflags |= KRB5_KDB_DISALLOW_PROXIABLE;
            else
                goto err_usage;

            mask |= LDAP_POLICY_TKTFLAGS;
        } else if (!strcmp((argv[i] + 1), "allow_dup_skey")) {
            if (*(argv[i]) == '+')
                policyparams->tktflags &= (int)(~KRB5_KDB_DISALLOW_DUP_SKEY);
            else if (*(argv[i]) == '-')
                policyparams->tktflags |= KRB5_KDB_DISALLOW_DUP_SKEY;
            else
                goto err_usage;

            mask |= LDAP_POLICY_TKTFLAGS;
        } else if (!strcmp((argv[i] + 1), "requires_preauth")) {
            if (*(argv[i]) == '+')
                policyparams->tktflags |= KRB5_KDB_REQUIRES_PRE_AUTH;
            else if (*(argv[i]) == '-')
                policyparams->tktflags &= (int)(~KRB5_KDB_REQUIRES_PRE_AUTH);
            else
                goto err_usage;

            mask |= LDAP_POLICY_TKTFLAGS;
        } else if (!strcmp((argv[i] + 1), "requires_hwauth")) {
            if (*(argv[i]) == '+')
                policyparams->tktflags |= KRB5_KDB_REQUIRES_HW_AUTH;
            else if (*(argv[i]) == '-')
                policyparams->tktflags &= (int)(~KRB5_KDB_REQUIRES_HW_AUTH);
            else
                goto err_usage;

            mask |= LDAP_POLICY_TKTFLAGS;
        } else if (!strcmp((argv[i] + 1), "allow_svr")) {
            if (*(argv[i]) == '+')
                policyparams->tktflags &= (int)(~KRB5_KDB_DISALLOW_SVR);
            else if (*(argv[i]) == '-')
                policyparams->tktflags |= KRB5_KDB_DISALLOW_SVR;
            else
                goto err_usage;

            mask |= LDAP_POLICY_TKTFLAGS;
        } else if (!strcmp((argv[i] + 1), "allow_tgs_req")) {
            if (*(argv[i]) == '+')
                policyparams->tktflags &= (int)(~KRB5_KDB_DISALLOW_TGT_BASED);
            else if (*(argv[i]) == '-')
                policyparams->tktflags |= KRB5_KDB_DISALLOW_TGT_BASED;
            else
                goto err_usage;

            mask |= LDAP_POLICY_TKTFLAGS;
        } else if (!strcmp((argv[i] + 1), "allow_tix")) {
            if (*(argv[i]) == '+')
                policyparams->tktflags &= (int)(~KRB5_KDB_DISALLOW_ALL_TIX);
            else if (*(argv[i]) == '-')
                policyparams->tktflags |= KRB5_KDB_DISALLOW_ALL_TIX;
            else
                goto err_usage;

            mask |= LDAP_POLICY_TKTFLAGS;
        } else if (!strcmp((argv[i] + 1), "needchange")) {
            if (*(argv[i]) == '+')
                policyparams->tktflags |= KRB5_KDB_REQUIRES_PWCHANGE;
            else if (*(argv[i]) == '-')
                policyparams->tktflags &= (int)(~KRB5_KDB_REQUIRES_PWCHANGE);
            else
                goto err_usage;

            mask |= LDAP_POLICY_TKTFLAGS;
        } else if (!strcmp((argv[i] + 1), "password_changing_service")) {
            if (*(argv[i]) == '+')
                policyparams->tktflags |= KRB5_KDB_PWCHANGE_SERVICE;
            else if (*(argv[i]) == '-')
                policyparams->tktflags &= (int)(~KRB5_KDB_PWCHANGE_SERVICE);
            else
                goto err_usage;

            mask |= LDAP_POLICY_TKTFLAGS;
        } else { /* Any other argument must be policy DN */
            /* First check if policy DN is already provided --
               if so, there's a usage error */
            if (policyparams->policy != NULL)
                goto err_usage;

            /* If not present already, fill up policy DN */
            policyparams->policy = strdup(argv[i]);
            if (policyparams->policy == NULL) {
                retval = ENOMEM;
                com_err(me, retval, _("while creating policy object"));
                goto err_nomsg;
            }
        }
    }

    /* policy DN is a mandatory argument. If not provided, print usage */
    if (policyparams->policy == NULL)
        goto err_usage;

    if ((retval = init_ldap_realm (argc, argv))) {
        com_err(me, retval, _("while reading realm information"));
        goto err_nomsg;
    }

    /* Create object with all attributes provided */
    if ((retval = krb5_ldap_create_policy(util_context, policyparams, mask)) != 0)
        goto cleanup;

    goto cleanup;

err_usage:
    print_usage = TRUE;

err_nomsg:
    no_msg = TRUE;

cleanup:
    /* Clean-up structure */
    krb5_ldap_free_policy (util_context, policyparams);

    if (print_usage)
        db_usage(CREATE_POLICY);

    if (retval) {
        if (!no_msg)
            com_err(me, retval, _("while creating policy object"));

        exit_status++;
    }

    return;
}


/*
 * This function will destroy the specified ticket policy
 * object interactively, unless forced through an option.
 */
void
kdb5_ldap_destroy_policy(int argc, char *argv[])
{
    char *me = progname;
    krb5_error_code retval = 0;
    krb5_ldap_policy_params *policyparams = NULL;
    krb5_boolean print_usage = FALSE;
    krb5_boolean no_msg = FALSE;
    char *policy = NULL;
    int mask = 0;
    int force = 0;
    char buf[5] = {0};
    int i = 0;

    if ((argc < 2) || (argc > 3)) {
        goto err_usage;
    }

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-force") == 0) {
            force++;
        } else { /* Any other argument must be policy DN */
            /* First check if policy DN is already provided --
               if so, there's a usage error */
            if (policy != NULL)
                goto err_usage;

            /* If not present already, fill up policy DN */
            policy = strdup(argv[i]);
            if (policy == NULL) {
                retval = ENOMEM;
                com_err(me, retval, _("while destroying policy object"));
                goto err_nomsg;
            }
        }
    }

    if (policy == NULL)
        goto err_usage;

    if (!force) {
        printf(_("This will delete the policy object '%s', are you sure?\n"),
               policy);
        printf(_("(type 'yes' to confirm)? "));

        if (fgets(buf, sizeof(buf), stdin) == NULL) {
            retval = EINVAL;
            goto cleanup;
        }

        if (strcmp(buf, yes)) {
            exit_status++;
            goto cleanup;
        }
    }

    if ((retval = init_ldap_realm (argc, argv)))
        goto err_nomsg;

    if ((retval = krb5_ldap_read_policy(util_context, policy, &policyparams, &mask)))
        goto cleanup;


    if ((retval = krb5_ldap_delete_policy(util_context, policy)))
        goto cleanup;

    printf("** policy object '%s' deleted.\n", policy);
    goto cleanup;


err_usage:
    print_usage = TRUE;

err_nomsg:
    no_msg = TRUE;

cleanup:
    /* Clean-up structure */
    krb5_ldap_free_policy (util_context, policyparams);

    if (policy) {
        free (policy);
    }

    if (print_usage) {
        db_usage(DESTROY_POLICY);
    }

    if (retval) {
        if (!no_msg)
            com_err(me, retval, _("while destroying policy object"));

        exit_status++;
    }

    return;
}


/*
 * This function will modify the attributes of a given ticket
 * policy object.
 */
void
kdb5_ldap_modify_policy(int argc, char *argv[])
{
    char *me = progname;
    krb5_error_code retval = 0;
    krb5_ldap_policy_params *policyparams = NULL;
    krb5_boolean print_usage = FALSE;
    krb5_boolean no_msg = FALSE;
    char *policy = NULL;
    int in_mask = 0, out_mask = 0;
    time_t date = 0;
    time_t now = 0;
    int i = 0;

    /* Check for number of arguments -- minimum is 3
       since atleast one parameter should be given in
       addition to 'modify_policy' and policy DN */
    if ((argc < 3) || (argc > 16)) {
        goto err_usage;
    }

    /* Parse all arguments, only to pick up policy DN (Pass 1) */
    for (i = 1; i < argc; i++) {
        /* Skip arguments next to 'maxtktlife'
           and 'maxrenewlife' arguments */
        if (!strcmp(argv[i], "-maxtktlife")) {
            ++i;
        } else if (!strcmp(argv[i], "-maxrenewlife")) {
            ++i;
        }
        /* Do nothing for ticket flag arguments */
        else if (!strcmp((argv[i] + 1), "allow_postdated") ||
                 !strcmp((argv[i] + 1), "allow_forwardable") ||
                 !strcmp((argv[i] + 1), "allow_renewable") ||
                 !strcmp((argv[i] + 1), "allow_proxiable") ||
                 !strcmp((argv[i] + 1), "allow_dup_skey") ||
                 !strcmp((argv[i] + 1), "requires_preauth") ||
                 !strcmp((argv[i] + 1), "requires_hwauth") ||
                 !strcmp((argv[i] + 1), "allow_svr") ||
                 !strcmp((argv[i] + 1), "allow_tgs_req") ||
                 !strcmp((argv[i] + 1), "allow_tix") ||
                 !strcmp((argv[i] + 1), "needchange") ||
                 !strcmp((argv[i] + 1), "password_changing_service")) {
        } else { /* Any other argument must be policy DN */
            /* First check if policy DN is already provided --
               if so, there's a usage error */
            if (policy != NULL)
                goto err_usage;

            /* If not present already, fill up policy DN */
            policy = strdup(argv[i]);
            if (policy == NULL) {
                retval = ENOMEM;
                com_err(me, retval, _("while modifying policy object"));
                goto err_nomsg;
            }
        }
    }

    if (policy == NULL)
        goto err_usage;

    if ((retval = init_ldap_realm (argc, argv)))
        goto cleanup;

    retval = krb5_ldap_read_policy(util_context, policy, &policyparams, &in_mask);
    if (retval) {
        com_err(me, retval, _("while reading information of policy '%s'"),
                policy);
        goto err_nomsg;
    }

    /* Get current time */
    time (&now);

    /* Parse all arguments, but skip policy DN (Pass 2) */
    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-maxtktlife")) {
            if (++i > argc - 1)
                goto err_usage;

            date = get_date(argv[i]);
            if (date == (time_t)(-1)) {
                retval = EINVAL;
                com_err(me, retval, _("while providing time specification"));
                goto err_nomsg;
            }

            policyparams->maxtktlife = date - now;

            out_mask |= LDAP_POLICY_MAXTKTLIFE;
        } else if (!strcmp(argv[i], "-maxrenewlife")) {
            if (++i > argc - 1)
                goto err_usage;

            date = get_date(argv[i]);
            if (date == (time_t)(-1)) {
                retval = EINVAL;
                com_err(me, retval, _("while providing time specification"));
                goto err_nomsg;
            }

            policyparams->maxrenewlife = date - now;

            out_mask |= LDAP_POLICY_MAXRENEWLIFE;
        } else if (!strcmp((argv[i] + 1), "allow_postdated")) {
            if (*(argv[i]) == '+')
                policyparams->tktflags &= (int)(~KRB5_KDB_DISALLOW_POSTDATED);
            else if (*(argv[i]) == '-')
                policyparams->tktflags |= KRB5_KDB_DISALLOW_POSTDATED;
            else
                goto err_usage;

            out_mask |= LDAP_POLICY_TKTFLAGS;
        } else if (!strcmp((argv[i] + 1), "allow_forwardable")) {
            if (*(argv[i]) == '+')
                policyparams->tktflags &= (int)(~KRB5_KDB_DISALLOW_FORWARDABLE);
            else if (*(argv[i]) == '-')
                policyparams->tktflags |= KRB5_KDB_DISALLOW_FORWARDABLE;
            else
                goto err_usage;

            out_mask |= LDAP_POLICY_TKTFLAGS;
        } else if (!strcmp((argv[i] + 1), "allow_renewable")) {
            if (*(argv[i]) == '+')
                policyparams->tktflags &= (int)(~KRB5_KDB_DISALLOW_RENEWABLE);
            else if (*(argv[i]) == '-')
                policyparams->tktflags |= KRB5_KDB_DISALLOW_RENEWABLE;
            else
                goto err_usage;

            out_mask |= LDAP_POLICY_TKTFLAGS;
        } else if (!strcmp((argv[i] + 1), "allow_proxiable")) {
            if (*(argv[i]) == '+')
                policyparams->tktflags &= (int)(~KRB5_KDB_DISALLOW_PROXIABLE);
            else if (*(argv[i]) == '-')
                policyparams->tktflags |= KRB5_KDB_DISALLOW_PROXIABLE;
            else
                goto err_usage;

            out_mask |= LDAP_POLICY_TKTFLAGS;
        } else if (!strcmp((argv[i] + 1), "allow_dup_skey")) {
            if (*(argv[i]) == '+')
                policyparams->tktflags &= (int)(~KRB5_KDB_DISALLOW_DUP_SKEY);
            else if (*(argv[i]) == '-')
                policyparams->tktflags |= KRB5_KDB_DISALLOW_DUP_SKEY;
            else
                goto err_usage;

            out_mask |= LDAP_POLICY_TKTFLAGS;
        } else if (!strcmp((argv[i] + 1), "requires_preauth")) {
            if (*(argv[i]) == '+')
                policyparams->tktflags |= KRB5_KDB_REQUIRES_PRE_AUTH;
            else if (*(argv[i]) == '-')
                policyparams->tktflags &= (int)(~KRB5_KDB_REQUIRES_PRE_AUTH);
            else
                goto err_usage;

            out_mask |= LDAP_POLICY_TKTFLAGS;
        } else if (!strcmp((argv[i] + 1), "requires_hwauth")) {
            if (*(argv[i]) == '+')
                policyparams->tktflags |= KRB5_KDB_REQUIRES_HW_AUTH;
            else if (*(argv[i]) == '-')
                policyparams->tktflags &= (int)(~KRB5_KDB_REQUIRES_HW_AUTH);
            else
                goto err_usage;

            out_mask |= LDAP_POLICY_TKTFLAGS;
        } else if (!strcmp((argv[i] + 1), "allow_svr")) {
            if (*(argv[i]) == '+')
                policyparams->tktflags &= (int)(~KRB5_KDB_DISALLOW_SVR);
            else if (*(argv[i]) == '-')
                policyparams->tktflags |= KRB5_KDB_DISALLOW_SVR;
            else
                goto err_usage;

            out_mask |= LDAP_POLICY_TKTFLAGS;
        } else if (!strcmp((argv[i] + 1), "allow_tgs_req")) {
            if (*(argv[i]) == '+')
                policyparams->tktflags &= (int)(~KRB5_KDB_DISALLOW_TGT_BASED);
            else if (*(argv[i]) == '-')
                policyparams->tktflags |= KRB5_KDB_DISALLOW_TGT_BASED;
            else
                goto err_usage;

            out_mask |= LDAP_POLICY_TKTFLAGS;
        } else if (!strcmp((argv[i] + 1), "allow_tix")) {
            if (*(argv[i]) == '+')
                policyparams->tktflags &= (int)(~KRB5_KDB_DISALLOW_ALL_TIX);
            else if (*(argv[i]) == '-')
                policyparams->tktflags |= KRB5_KDB_DISALLOW_ALL_TIX;
            else
                goto err_usage;

            out_mask |= LDAP_POLICY_TKTFLAGS;
        } else if (!strcmp((argv[i] + 1), "needchange")) {
            if (*(argv[i]) == '+')
                policyparams->tktflags |= KRB5_KDB_REQUIRES_PWCHANGE;
            else if (*(argv[i]) == '-')
                policyparams->tktflags &= (int)(~KRB5_KDB_REQUIRES_PWCHANGE);
            else
                goto err_usage;

            out_mask |= LDAP_POLICY_TKTFLAGS;
        } else if (!strcmp((argv[i] + 1), "password_changing_service")) {
            if (*(argv[i]) == '+')
                policyparams->tktflags |= KRB5_KDB_PWCHANGE_SERVICE;
            else if (*(argv[i]) == '-')
                policyparams->tktflags &= (int)(~KRB5_KDB_PWCHANGE_SERVICE);
            else
                goto err_usage;

            out_mask |= LDAP_POLICY_TKTFLAGS;
        } else {
            /* Any other argument must be policy DN
               -- skip it */
        }
    }

    /* Modify attributes of object */
    if ((retval = krb5_ldap_modify_policy(util_context, policyparams, out_mask)))
        goto cleanup;

    goto cleanup;

err_usage:
    print_usage = TRUE;

err_nomsg:
    no_msg = TRUE;

cleanup:
    /* Clean-up structure */
    krb5_ldap_free_policy (util_context, policyparams);

    if (policy)
        free (policy);

    if (print_usage)
        db_usage(MODIFY_POLICY);

    if (retval) {
        if (!no_msg)
            com_err(me, retval, _("while modifying policy object"));

        exit_status++;
    }

    return;
}


/*
 * This function will display information about the given policy object,
 * fetching the information from the LDAP Server.
 */
void
kdb5_ldap_view_policy(int argc, char *argv[])
{
    char *me = progname;
    krb5_ldap_policy_params *policyparams = NULL;
    krb5_error_code retval = 0;
    krb5_boolean print_usage = FALSE;
    char *policy = NULL;
    int mask = 0;

    if (argc != 2) {
        goto err_usage;
    }

    policy = strdup(argv[1]);
    if (policy == NULL) {
        com_err(me, ENOMEM, _("while viewing policy"));
        exit_status++;
        goto cleanup;
    }

    if ((retval = init_ldap_realm (argc, argv)))
        goto cleanup;

    if ((retval = krb5_ldap_read_policy(util_context, policy, &policyparams, &mask))) {
        com_err(me, retval, _("while viewing policy '%s'"), policy);
        exit_status++;
        goto cleanup;
    }

    print_policy_params (policyparams, mask);

    goto cleanup;

err_usage:
    print_usage = TRUE;

cleanup:
    krb5_ldap_free_policy (util_context, policyparams);

    if (policy)
        free (policy);

    if (print_usage) {
        db_usage(VIEW_POLICY);
    }

    return;
}


/*
 * This function will print the policy object information to the
 * standard output.
 */
static void
print_policy_params(krb5_ldap_policy_params *policyparams, int mask)
{
    /* Print the policy DN */
    printf("%25s: %s\n", "Ticket policy", policyparams->policy);

    /* Print max. ticket life and max. renewable life, if present */
    if (mask & LDAP_POLICY_MAXTKTLIFE)
        printf("%25s: %s\n", "Maximum ticket life", strdur(policyparams->maxtktlife));
    if (mask & LDAP_POLICY_MAXRENEWLIFE)
        printf("%25s: %s\n", "Maximum renewable life", strdur(policyparams->maxrenewlife));

    /* Service flags are printed */
    printf("%25s: ", "Ticket flags");
    if (mask & LDAP_POLICY_TKTFLAGS) {
        int ticketflags = policyparams->tktflags;

        if (ticketflags & KRB5_KDB_DISALLOW_POSTDATED)
            printf("%s ","DISALLOW_POSTDATED");

        if (ticketflags & KRB5_KDB_DISALLOW_FORWARDABLE)
            printf("%s ","DISALLOW_FORWARDABLE");

        if (ticketflags & KRB5_KDB_DISALLOW_RENEWABLE)
            printf("%s ","DISALLOW_RENEWABLE");

        if (ticketflags & KRB5_KDB_DISALLOW_PROXIABLE)
            printf("%s ","DISALLOW_PROXIABLE");

        if (ticketflags & KRB5_KDB_DISALLOW_DUP_SKEY)
            printf("%s ","DISALLOW_DUP_SKEY");

        if (ticketflags & KRB5_KDB_REQUIRES_PRE_AUTH)
            printf("%s ","REQUIRES_PRE_AUTH");

        if (ticketflags & KRB5_KDB_REQUIRES_HW_AUTH)
            printf("%s ","REQUIRES_HW_AUTH");

        if (ticketflags & KRB5_KDB_DISALLOW_SVR)
            printf("%s ","DISALLOW_SVR");

        if (ticketflags & KRB5_KDB_DISALLOW_TGT_BASED)
            printf("%s ","DISALLOW_TGT_BASED");

        if (ticketflags & KRB5_KDB_DISALLOW_ALL_TIX)
            printf("%s ","DISALLOW_ALL_TIX");

        if (ticketflags & KRB5_KDB_REQUIRES_PWCHANGE)
            printf("%s ","REQUIRES_PWCHANGE");

        if (ticketflags & KRB5_KDB_PWCHANGE_SERVICE)
            printf("%s ","PWCHANGE_SERVICE");
    }
    printf("\n");

    return;
}


/*
 * This function will list the DNs of policy objects under a specific
 * sub-tree (entire tree by default)
 */
void
kdb5_ldap_list_policies(int argc, char *argv[])
{
    char *me = progname;
    krb5_error_code retval = 0;
    krb5_boolean print_usage = FALSE;
    char **list = NULL;
    char **plist = NULL;

    /* Check for number of arguments */
    if ((argc != 1) && (argc != 3)) {
        goto err_usage;
    }

    if ((retval = init_ldap_realm (argc, argv)))
        goto cleanup;

    retval = krb5_ldap_list_policy(util_context, NULL, &list);
    if ((retval != 0) || (list == NULL))
        goto cleanup;

    for (plist = list; *plist != NULL; plist++) {
        printf("%s\n", *plist);
    }

    goto cleanup;

err_usage:
    print_usage = TRUE;

cleanup:
    if (list != NULL) {
        krb5_free_list_entries (list);
        free (list);
    }

    if (print_usage) {
        db_usage(LIST_POLICY);
    }

    if (retval) {
        com_err(me, retval, _("while listing policy objects"));
        exit_status++;
    }

    return;
}


/* Reproduced from kadmin.c, instead of linking
   the entire kadmin.o */
static char *
strdur(time_t duration)
{
    static char out[50];
    int neg, days, hours, minutes, seconds;

    if (duration < 0) {
        duration *= -1;
        neg = 1;
    } else
        neg = 0;
    days = duration / (24 * 3600);
    duration %= 24 * 3600;
    hours = duration / 3600;
    duration %= 3600;
    minutes = duration / 60;
    duration %= 60;
    seconds = duration;
    snprintf(out, sizeof(out), "%s%d %s %02d:%02d:%02d", neg ? "-" : "",
             days, days == 1 ? "day" : "days", hours, minutes, seconds);
    return out;
}
