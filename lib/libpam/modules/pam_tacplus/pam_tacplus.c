/*-
 * Copyright 1998 Juniper Networks, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$FreeBSD: src/lib/libpam/modules/pam_tacplus/pam_tacplus.c,v 1.2 1999/01/20 21:55:28 jdp Exp $
 */

#include <sys/param.h>

#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <taclib.h>
#include <unistd.h>

#define PAM_SM_AUTH
#include <security/pam_modules.h>

#include "pam_mod_misc.h"

/* Option names, including the "=" sign. */
#define OPT_CONF		"conf="
#define OPT_TMPL		"template_user="

typedef int (*set_func)(struct tac_handle *, const char *);

static int	 do_item(pam_handle_t *, struct tac_handle *, int,
		    set_func, const char *);
static char	*get_msg(struct tac_handle *);
static int	 set_msg(struct tac_handle *, const char *);

static int
do_item(pam_handle_t *pamh, struct tac_handle *tach, int item,
    set_func func, const char *funcname)
{
	int retval;
	const void *value;

	if ((retval = pam_get_item(pamh, item, &value)) != PAM_SUCCESS)
	    return retval;
	if (value != NULL && (*func)(tach, (const char *)value) == -1) {
		syslog(LOG_CRIT, "%s: %s", funcname, tac_strerror(tach));
		tac_close(tach);
		return PAM_SERVICE_ERR;
	}
	return PAM_SUCCESS;
}

static char *
get_msg(struct tac_handle *tach)
{
	char *msg;

	if ((msg = tac_get_msg(tach)) == NULL) {
		syslog(LOG_CRIT, "tac_get_msg: %s", tac_strerror(tach));
		tac_close(tach);
		return NULL;
	}
	return msg;
}

static int
set_msg(struct tac_handle *tach, const char *msg)
{
	if (tac_set_msg(tach, msg) == -1) {
		syslog(LOG_CRIT, "tac_set_msg: %s", tac_strerror(tach));
		tac_close(tach);
		return -1;
	}
	return 0;
}

PAM_EXTERN int
pam_sm_authenticate(pam_handle_t *pamh, int flags, int argc,
    const char **argv)
{
	int retval;
	struct tac_handle *tach;
	const char *conf_file = NULL;
	const char *template_user = NULL;
	int options = 0;
	int i;

	for (i = 0;  i < argc;  i++) {
		size_t len;

		pam_std_option(&options, argv[i]);
		if (strncmp(argv[i], OPT_CONF, (len = strlen(OPT_CONF))) == 0)
			conf_file = argv[i] + len;
		else if (strncmp(argv[i], OPT_TMPL,
		    (len = strlen(OPT_TMPL))) == 0)
			template_user = argv[i] + len;
	}

	if ((tach = tac_open()) == NULL) {
		syslog(LOG_CRIT, "tac_open failed");
		return PAM_SERVICE_ERR;
	}
	if (tac_config(tach, conf_file) == -1) {
		syslog(LOG_ALERT, "tac_config: %s", tac_strerror(tach));
		tac_close(tach);
		return PAM_SERVICE_ERR;
	}
	if (tac_create_authen(tach, TAC_AUTHEN_LOGIN, TAC_AUTHEN_TYPE_ASCII,
	    TAC_AUTHEN_SVC_LOGIN) == -1) {
		syslog(LOG_CRIT, "tac_create_authen: %s", tac_strerror(tach));
		tac_close(tach);
		return PAM_SERVICE_ERR;
	}
	if ((retval = do_item(pamh, tach, PAM_USER,
	    tac_set_user, "tac_set_user")) != PAM_SUCCESS)
		return retval;
	if ((retval = do_item(pamh, tach, PAM_TTY,
	    tac_set_port, "tac_set_port")) != PAM_SUCCESS)
		return retval;
	if ((retval = do_item(pamh, tach, PAM_RHOST,
	    tac_set_rem_addr, "tac_set_rem_addr")) != PAM_SUCCESS)
		return retval;
	for ( ; ; ) {
		char *srvr_msg;
		size_t msg_len;
		const char *user_msg;
		char *data_msg;
		int sflags;
		int status;
		int echo;

		if ((sflags = tac_send_authen(tach)) == -1) {
			syslog(LOG_CRIT, "tac_send_authen: %s",
			    tac_strerror(tach));
			tac_close(tach);
			return PAM_AUTHINFO_UNAVAIL;
		}
		status = TAC_AUTHEN_STATUS(sflags);
		echo = TAC_AUTHEN_NOECHO(sflags) ? 0 : PAM_OPT_ECHO_PASS;
		switch (status) {

		case TAC_AUTHEN_STATUS_PASS:
			tac_close(tach);
			if (template_user != NULL) {
				const void *item;
				const char *user;

				/*
				 * If the given user name doesn't exist in
				 * the local password database, change it
				 * to the value given in the "template_user"
				 * option.
				 */
				retval = pam_get_item(pamh, PAM_USER, &item);
				if (retval != PAM_SUCCESS)
					return retval;
				user = (const char *)item;
				if (getpwnam(user) == NULL)
					pam_set_item(pamh, PAM_USER,
					    template_user);
			}
			return PAM_SUCCESS;

		case TAC_AUTHEN_STATUS_FAIL:
			tac_close(tach);
			return PAM_AUTH_ERR;

		case TAC_AUTHEN_STATUS_GETUSER:
		case TAC_AUTHEN_STATUS_GETPASS:
			if ((srvr_msg = get_msg(tach)) == NULL)
				return PAM_SERVICE_ERR;
			if (status == TAC_AUTHEN_STATUS_GETUSER)
				retval = pam_get_user(pamh, &user_msg,
				    srvr_msg[0] != '\0' ? srvr_msg : NULL);
			else if (status == TAC_AUTHEN_STATUS_GETPASS)
				retval = pam_get_pass(pamh, &user_msg,
				    srvr_msg[0] != '\0' ? srvr_msg :
				    "Password:", options | echo);
			free(srvr_msg);
			if (retval != PAM_SUCCESS) {
				/* XXX - send a TACACS+ abort packet */
				tac_close(tach);
				return retval;
			}
			if (set_msg(tach, user_msg) == -1)
				return PAM_SERVICE_ERR;
			break;

		case TAC_AUTHEN_STATUS_GETDATA:
			if ((srvr_msg = get_msg(tach)) == NULL)
				return PAM_SERVICE_ERR;
			retval = pam_prompt(pamh,
			    (options|echo) & PAM_OPT_ECHO_PASS ?
			    PAM_PROMPT_ECHO_ON : PAM_PROMPT_ECHO_OFF,
			    srvr_msg[0] != '\0' ? srvr_msg : "Data:",
			    &data_msg);
			free(srvr_msg);
			if (retval != PAM_SUCCESS) {
				/* XXX - send a TACACS+ abort packet */
				tac_close(tach);
				return retval;
			}
			retval = set_msg(tach, data_msg);
			memset(data_msg, 0, strlen(data_msg));
			free(data_msg);
			if (retval == -1)
				return PAM_SERVICE_ERR;
			break;

		case TAC_AUTHEN_STATUS_ERROR:
			srvr_msg = (char *)tac_get_data(tach, &msg_len);
			if (srvr_msg != NULL && msg_len != 0) {
				syslog(LOG_CRIT, "tac_send_authen:"
				    " server detected error: %s", srvr_msg);
				free(srvr_msg);
			} else
				syslog(LOG_CRIT,
				    "tac_send_authen: server detected error");
			tac_close(tach);
			return PAM_AUTHINFO_UNAVAIL;
			break;

		case TAC_AUTHEN_STATUS_RESTART:
		case TAC_AUTHEN_STATUS_FOLLOW:
		default:
			syslog(LOG_CRIT,
			    "tac_send_authen: unexpected status %#x", status);
			tac_close(tach);
			return PAM_AUTHINFO_UNAVAIL;
		}
	}
}

PAM_EXTERN int
pam_sm_setcred(pam_handle_t *pamh, int flags, int argc, const char **argv)
{
	return PAM_SUCCESS;
}

PAM_MODULE_ENTRY("pam_tacplus");
