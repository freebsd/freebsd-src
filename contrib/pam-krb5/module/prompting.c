/*
 * Prompt users for information.
 *
 * Handles all interaction with the PAM conversation, either directly or
 * indirectly through the Kerberos libraries.
 *
 * Copyright 2005-2007, 2009, 2014, 2017, 2020 Russ Allbery <eagle@eyrie.org>
 * Copyright 2011-2012
 *     The Board of Trustees of the Leland Stanford Junior University
 * Copyright 2005 Andres Salomon <dilinger@debian.org>
 * Copyright 1999-2000 Frank Cusack <fcusack@fcusack.com>
 *
 * SPDX-License-Identifier: BSD-3-clause or GPL-1+
 */

#include <config.h>
#include <portable/krb5.h>
#include <portable/pam.h>
#include <portable/system.h>

#include <assert.h>
#include <errno.h>

#include <module/internal.h>
#include <pam-util/args.h>
#include <pam-util/logging.h>


/*
 * Build a password prompt.
 *
 * The default prompt is simply "Password:".  Optionally, a string describing
 * the type of password is passed in as prefix.  In this case, the prompts is:
 *
 *     <prefix> <banner> password:
 *
 * where <prefix> is the argument passed and <banner> is the value of
 * args->banner (defaulting to "Kerberos").
 *
 * If args->config->expose_account is set, we append the principal name (taken
 * from args->config->ctx->princ) before the colon, so the prompts are:
 *
 *     Password for <principal>:
 *     <prefix> <banner> password for <principal>:
 *
 * Normally this is not done because it exposes the realm and possibly any
 * username to principal mappings, plus may confuse some ssh clients if sshd
 * passes the prompt back to the client.
 *
 * Returns newly-allocated memory or NULL on failure.  The caller is
 * responsible for freeing.
 */
static char *
build_password_prompt(struct pam_args *args, const char *prefix)
{
    struct context *ctx = args->config->ctx;
    char *principal = NULL;
    const char *banner, *bspace;
    char *prompt, *tmp;
    bool expose_account;
    krb5_error_code k5_errno;
    int retval;

    /* If we're exposing the account, format the principal name. */
    if (args->config->expose_account || prefix != NULL)
        if (ctx != NULL && ctx->context != NULL && ctx->princ != NULL) {
            k5_errno = krb5_unparse_name(ctx->context, ctx->princ, &principal);
            if (k5_errno != 0)
                putil_debug_krb5(args, k5_errno, "krb5_unparse_name failed");
        }

    /* Build the part of the prompt without the principal name. */
    if (prefix == NULL)
        tmp = strdup("Password");
    else {
        banner = (args->config->banner == NULL) ? "" : args->config->banner;
        bspace = (args->config->banner == NULL) ? "" : " ";
        retval = asprintf(&tmp, "%s%s%s password", prefix, bspace, banner);
        if (retval < 0)
            tmp = NULL;
    }
    if (tmp == NULL)
        goto fail;

    /* Add the principal, if desired, and the colon and space. */
    expose_account = args->config->expose_account && principal != NULL;
    if (expose_account)
        retval = asprintf(&prompt, "%s for %s: ", tmp, principal);
    else
        retval = asprintf(&prompt, "%s: ", tmp);
    free(tmp);
    if (retval < 0)
        goto fail;

    /* Clean up and return. */
    if (principal != NULL)
        krb5_free_unparsed_name(ctx->context, principal);
    return prompt;

fail:
    if (principal != NULL)
        krb5_free_unparsed_name(ctx->context, principal);
    return NULL;
}


/*
 * Prompt for a password.
 *
 * The entered password is stored in password.  The memory is allocated by the
 * application and returned as part of the PAM conversation.  It must be freed
 * by the caller.
 *
 * Returns a PAM success or error code.
 */
int
pamk5_get_password(struct pam_args *args, const char *prefix, char **password)
{
    char *prompt;
    int retval;

    prompt = build_password_prompt(args, prefix);
    if (prompt == NULL)
        return PAM_BUF_ERR;
    retval = pamk5_conv(args, prompt, PAM_PROMPT_ECHO_OFF, password);
    free(prompt);
    return retval;
}


/*
 * Get information from the user or display a message to the user, as
 * determined by type.  If PAM_SILENT was given, don't pass any text or error
 * messages to the application.
 *
 * The response variable is set to the response returned by the conversation
 * function on a successful return if a response was desired.  Caller is
 * responsible for freeing it.
 */
int
pamk5_conv(struct pam_args *args, const char *message, int type,
           char **response)
{
    int pamret;
    struct pam_message msg;
    PAM_CONST struct pam_message *pmsg;
    struct pam_response *resp = NULL;
    struct pam_conv *conv;
    int want_reply;

    if (args->silent && (type == PAM_ERROR_MSG || type == PAM_TEXT_INFO))
        return PAM_SUCCESS;
    pamret = pam_get_item(args->pamh, PAM_CONV, (PAM_CONST void **) &conv);
    if (pamret != PAM_SUCCESS)
        return pamret;
    if (conv->conv == NULL)
        return PAM_CONV_ERR;
    pmsg = &msg;
    msg.msg_style = type;
    msg.msg = (char *) message;
    pamret = conv->conv(1, &pmsg, &resp, conv->appdata_ptr);
    if (pamret != PAM_SUCCESS)
        return pamret;

    /*
     * Only expect a response for PAM_PROMPT_ECHO_OFF or PAM_PROMPT_ECHO_ON
     * message types.  This mildly annoying logic makes sure that everything
     * is freed properly (except the response itself, if wanted, which is
     * returned for the caller to free) and that the success status is set
     * based on whether the reply matched our expectations.
     *
     * If we got a reply even though we didn't want one, still overwrite the
     * reply before freeing in case it was a password.
     */
    want_reply = (type == PAM_PROMPT_ECHO_OFF || type == PAM_PROMPT_ECHO_ON);
    if (resp == NULL || resp->resp == NULL)
        pamret = want_reply ? PAM_CONV_ERR : PAM_SUCCESS;
    else if (want_reply && response != NULL) {
        *response = resp->resp;
        pamret = PAM_SUCCESS;
    } else {
        explicit_bzero(resp->resp, strlen(resp->resp));
        free(resp->resp);
        pamret = want_reply ? PAM_SUCCESS : PAM_CONV_ERR;
    }
    free(resp);
    return pamret;
}


/*
 * Allocate memory to copy all of the prompts into a pam_message.
 *
 * Linux PAM and Solaris PAM expect different things here.  Solaris PAM
 * expects to receive a pointer to a pointer to an array of pam_message
 * structs.  Linux PAM expects to receive a pointer to an array of pointers to
 * pam_message structs.  In order for the module to work with either PAM
 * implementation, we need to set up a structure that is valid either way you
 * look at it.
 *
 * We do this by making msg point to the array of struct pam_message pointers
 * (what Linux PAM expects), and then make the first one of those pointers
 * point to the array of pam_message structs.  Solaris will then be happy,
 * looking at only the first element of the outer array and finding it
 * pointing to the inner array.  Then, for Linux, we point the other elements
 * of the outer array to the storage allocated in the inner array.
 *
 * All this also means we have to be careful how we free the resulting
 * structure since it's double-linked in a subtle way.  Thankfully, we get to
 * free it ourselves.
 */
static struct pam_message **
allocate_pam_message(size_t total_prompts)
{
    struct pam_message **msg;
    size_t i;

    msg = calloc(total_prompts, sizeof(struct pam_message *));
    if (msg == NULL)
        return NULL;
    *msg = calloc(total_prompts, sizeof(struct pam_message));
    if (*msg == NULL) {
        free(msg);
        return NULL;
    }
    for (i = 1; i < total_prompts; i++)
        msg[i] = msg[0] + i;
    return msg;
}


/*
 * Free the structure created by allocate_pam_message.
 */
static void
free_pam_message(struct pam_message **msg, size_t total_prompts)
{
    size_t i;

    for (i = 0; i < total_prompts; i++)
        free((char *) msg[i]->msg);
    free(*msg);
    free(msg);
}


/*
 * Free the responses returned by the conversation function.  These may
 * contain passwords, so we overwrite them before we free them.
 */
static void
free_pam_responses(struct pam_response *resp, size_t total_prompts)
{
    size_t i;

    if (resp == NULL)
        return;
    for (i = 0; i < total_prompts; i++) {
        if (resp[i].resp != NULL) {
            explicit_bzero(resp[i].resp, strlen(resp[i].resp));
            free(resp[i].resp);
        }
    }
    free(resp);
}


/*
 * Format a Kerberos prompt into a PAM prompt.  Takes a krb5_prompt as input
 * and writes the resulting PAM prompt into a struct pam_message.
 */
static krb5_error_code
format_prompt(krb5_prompt *prompt, struct pam_message *message)
{
    size_t len = strlen(prompt->prompt);
    bool has_colon;
    const char *colon;
    int retval, style;

    /*
     * Heimdal adds the trailing colon and space, while MIT does not.
     * Work around the difference by looking to see if there's a trailing
     * colon and space already and only adding it if there is not.
     */
    has_colon = (len > 2 && memcmp(&prompt->prompt[len - 2], ": ", 2) == 0);
    colon = has_colon ? "" : ": ";
    retval = asprintf((char **) &message->msg, "%s%s", prompt->prompt, colon);
    if (retval < 0)
        return retval;
    style = prompt->hidden ? PAM_PROMPT_ECHO_OFF : PAM_PROMPT_ECHO_ON;
    message->msg_style = style;
    return 0;
}


/*
 * Given an array of struct pam_response elements, record the responses in the
 * corresponding krb5_prompt structures.
 */
static krb5_error_code
record_prompt_answers(struct pam_response *resp, int num_prompts,
                      krb5_prompt *prompts)
{
    int i;

    for (i = 0; i < num_prompts; i++) {
        size_t len, allowed;

        if (resp[i].resp == NULL)
            return KRB5_LIBOS_CANTREADPWD;
        len = strlen(resp[i].resp);
        allowed = prompts[i].reply->length;
        if (allowed == 0 || len > allowed - 1)
            return KRB5_LIBOS_CANTREADPWD;

        /*
         * Since the first version of this module, it has copied a nul
         * character into the prompt data buffer for MIT Kerberos with the
         * note that "other applications expect it to be there."  I suspect
         * this is incorrect and nothing cares about this nul, but have
         * preserved this behavior out of an abundance of caution.
         *
         * Note that it shortens the maximum response length we're willing to
         * accept by one (implemented above) and is the source of one prior
         * security vulnerability.
         */
        memcpy(prompts[i].reply->data, resp[i].resp, len + 1);
        prompts[i].reply->length = (unsigned int) len;
    }
    return 0;
}


/*
 * This is the generic prompting function called by both MIT Kerberos and
 * Heimdal prompting implementations.
 *
 * There are a lot of structures and different layers of code at work here,
 * making this code quite confusing.  This function is a prompter function to
 * pass into the Kerberos library, in particular krb5_get_init_creds_password.
 * It is used by the Kerberos library to prompt for a password if need be, and
 * also to prompt for password changes if the password was expired.
 *
 * The purpose of this function is to serve as glue between the Kerberos
 * library and the application (by way of the PAM glue).  PAM expects us to
 * pass back to the conversation function an array of prompts and receive from
 * the application an array of responses to those prompts.  We pass the
 * application an array of struct pam_message pointers, and the application
 * passes us an array of struct pam_response pointers.
 *
 * Kerberos, meanwhile, passes us in an array of krb5_prompt structs.  This
 * struct contains the prompt, a flag saying whether to suppress echoing of
 * what the user types for that prompt, and a buffer into which to store the
 * response.
 *
 * Therefore, what we're doing here is copying the prompts from the
 * krb5_prompt structs into pam_message structs, calling the conversation
 * function, and then copying the responses back out of pam_response structs
 * into the krb5_prompt structs to return to the Kerberos library.
 */
krb5_error_code
pamk5_prompter_krb5(krb5_context context UNUSED, void *data, const char *name,
                    const char *banner, int num_prompts, krb5_prompt *prompts)
{
    struct pam_args *args = data;
    int current_prompt, retval, pamret, i, offset;
    int total_prompts = num_prompts;
    struct pam_message **msg;
    struct pam_response *resp = NULL;
    struct pam_conv *conv;

    /* Treat the name and banner as prompts that doesn't need input. */
    if (name != NULL && !args->silent)
        total_prompts++;
    if (banner != NULL && !args->silent)
        total_prompts++;

    /* If we have zero prompts, do nothing, silently. */
    if (total_prompts == 0)
        return 0;

    /* Obtain the conversation function from the application. */
    pamret = pam_get_item(args->pamh, PAM_CONV, (PAM_CONST void **) &conv);
    if (pamret != 0)
        return KRB5_LIBOS_CANTREADPWD;
    if (conv->conv == NULL)
        return KRB5_LIBOS_CANTREADPWD;

    /* Allocate memory to copy all of the prompts into a pam_message. */
    msg = allocate_pam_message(total_prompts);
    if (msg == NULL)
        return ENOMEM;

    /* current_prompt is an index into msg and a count when we're done. */
    current_prompt = 0;
    if (name != NULL && !args->silent) {
        msg[current_prompt]->msg = strdup(name);
        if (msg[current_prompt]->msg == NULL) {
            retval = ENOMEM;
            goto cleanup;
        }
        msg[current_prompt]->msg_style = PAM_TEXT_INFO;
        current_prompt++;
    }
    if (banner != NULL && !args->silent) {
        assert(current_prompt < total_prompts);
        msg[current_prompt]->msg = strdup(banner);
        if (msg[current_prompt]->msg == NULL) {
            retval = ENOMEM;
            goto cleanup;
        }
        msg[current_prompt]->msg_style = PAM_TEXT_INFO;
        current_prompt++;
    }
    for (i = 0; i < num_prompts; i++) {
        assert(current_prompt < total_prompts);
        retval = format_prompt(&prompts[i], msg[current_prompt]);
        if (retval < 0)
            goto cleanup;
        current_prompt++;
    }

    /* Call into the application conversation function. */
    pamret = conv->conv(total_prompts, (PAM_CONST struct pam_message **) msg,
                        &resp, conv->appdata_ptr);
    if (pamret != 0 || resp == NULL) {
        retval = KRB5_LIBOS_CANTREADPWD;
        goto cleanup;
    }

    /*
     * Record the answers in the Kerberos data structure.  If name or banner
     * were provided, skip over the initial PAM responses that correspond to
     * those messages.
     */
    offset = 0;
    if (name != NULL && !args->silent)
        offset++;
    if (banner != NULL && !args->silent)
        offset++;
    retval = record_prompt_answers(resp + offset, num_prompts, prompts);

cleanup:
    free_pam_message(msg, total_prompts);
    free_pam_responses(resp, total_prompts);
    return retval;
}


/*
 * This is a special version of krb5_prompter_krb5 that returns an error if
 * the Kerberos library asks for a password.  It is only used with MIT
 * Kerberos as part of the implementation of try_pkinit and use_pkinit.
 * (Heimdal has a different API for PKINIT authentication.)
 */
#ifdef HAVE_KRB5_GET_PROMPT_TYPES
krb5_error_code
pamk5_prompter_krb5_no_password(krb5_context context, void *data,
                                const char *name, const char *banner,
                                int num_prompts, krb5_prompt *prompts)
{
    krb5_prompt_type *ptypes;
    int i;

    ptypes = krb5_get_prompt_types(context);
    for (i = 0; i < num_prompts; i++)
        if (ptypes != NULL && ptypes[i] == KRB5_PROMPT_TYPE_PASSWORD)
            return KRB5_LIBOS_CANTREADPWD;
    return pamk5_prompter_krb5(context, data, name, banner, num_prompts,
                               prompts);
}
#else  /* !HAVE_KRB5_GET_PROMPT_TYPES */
krb5_error_code
pamk5_prompter_krb5_no_password(krb5_context context, void *data,
                                const char *name, const char *banner,
                                int num_prompts, krb5_prompt *prompts)
{
    return pamk5_prompter_krb5(context, data, name, banner, num_prompts,
                               prompts);
}
#endif /* !HAVE_KRB5_GET_PROMPT_TYPES */
