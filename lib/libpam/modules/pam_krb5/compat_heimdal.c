/*
 * compat_heimdal.c
 *
 * Heimdal compatability layer.
 *
 * $FreeBSD$
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <krb5.h>
#include <security/pam_appl.h>
#include <security/pam_modules.h>
#include "pam_krb5.h"

const char *
compat_princ_component(krb5_context context, krb5_principal princ, int n)
{
	return princ->name.name_string.val[n];
}

void
compat_free_data_contents(krb5_context context, krb5_data *data)
{
	krb5_xfree(data->data);
}

krb5_error_code
compat_cc_next_cred(krb5_context context, const krb5_ccache id, 
    krb5_cc_cursor *cursor, krb5_creds *creds)
{
	return krb5_cc_next_cred(context, id, creds, cursor);
}


static krb5_error_code
heimdal_pam_prompter(krb5_context context, void *data, const char *banner, int 
  num_prompts, krb5_prompt prompts[])
{
    int		pam_prompts = num_prompts;
    int		pamret, i;

    struct pam_message	*msg;
    struct pam_response	*resp = NULL;
    struct pam_conv	*conv;
    pam_handle_t	*pamh = (pam_handle_t *) data;

    if ((pamret = pam_get_item(pamh, PAM_CONV, (const void **) &conv)) != 0)
	return KRB5KRB_ERR_GENERIC;

    if (banner)
	pam_prompts++;

    msg = calloc(sizeof(struct pam_message) * pam_prompts, 1);
    if (!msg)
	return ENOMEM;

    /* Now use pam_prompts as an index */
    pam_prompts = 0;

    if (banner) {
	msg[pam_prompts].msg = malloc(strlen(banner) + 1);
	if (!msg[pam_prompts].msg)
	    goto cleanup;
	strcpy((char *) msg[pam_prompts].msg, banner);
	msg[pam_prompts].msg_style = PAM_TEXT_INFO;
	pam_prompts++;
    }

    for (i = 0; i < num_prompts; i++) {
	msg[pam_prompts].msg = malloc(strlen(prompts[i].prompt) + 3);
	if (!msg[pam_prompts].msg)
	    goto cleanup;
	sprintf((char *) msg[pam_prompts].msg, "%s: ", prompts[i].prompt);
	msg[pam_prompts].msg_style = prompts[i].hidden ? PAM_PROMPT_ECHO_OFF
						       : PAM_PROMPT_ECHO_ON;
	pam_prompts++;
    }

    if ((pamret = conv->conv(pam_prompts, (const struct pam_message **) &msg, 
      &resp, conv->appdata_ptr)) != 0) 
	goto cleanup;

    if (!resp)
	goto cleanup;

    /* Reuse pam_prompts as a starting index */
    pam_prompts = 0;
    if (banner)
	pam_prompts++;

    for (i = 0; i < num_prompts; i++, pam_prompts++) {
	register int len;
	if (!resp[pam_prompts].resp) {
	    pamret = PAM_AUTH_ERR;
	    goto cleanup;
	}
	len = strlen(resp[pam_prompts].resp); /* Help out the compiler */
	if (len > prompts[i].reply->length) {
	    pamret = PAM_AUTH_ERR;
	    goto cleanup;
	}
	memcpy(prompts[i].reply->data, resp[pam_prompts].resp, len);
	prompts[i].reply->length = len;
    }

cleanup:
    /* pam_prompts is correct at this point */

    for (i = 0; i < pam_prompts; i++) {
	if (msg[i].msg)
	    free((char *) msg[i].msg);
    }
    free(msg);

    if (resp) {
	for (i = 0; i < pam_prompts; i++) {
	    /*
	     * Note that PAM is underspecified wrt free()'ing resp[i].resp.
	     * It's not clear if I should free it, or if the application
	     * has to. Therefore most (all?) apps won't free() it, and I
	     * can't either, as I am not sure it was malloc()'d. All PAM
	     * implementations I've seen leak memory here. Not so bad, IFF
	     * you fork/exec for each PAM authentication (as is typical).
	     */
#if 0
	    if (resp[i].resp)
		free(resp[i].resp);
#endif /* 0 */
	}
	/* This does not lose resp[i].resp if the application saved a copy. */
	free(resp);
    }

    return (pamret ? KRB5KRB_ERR_GENERIC : 0);
}

krb5_prompter_fct pam_prompter = heimdal_pam_prompter;
