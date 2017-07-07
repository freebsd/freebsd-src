/*
 * Copyright (c) 1997 Cygnus Solutions.
 *
 * Author:  Michael Graff
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "krb5.h"
#include "com_err.h"

#include "cns.h"

#include "../lib/gic.h"

/*
 * k5_change_password
 *
 * Use the new functions to change the password.
 */
krb5_error_code
k5_change_password(HWND hwnd, krb5_context context, char *user, char *realm,
		   char *opasswd, char *npasswd, char **text)
{
	krb5_error_code	           ret;
	krb5_data                  result_string;
	krb5_data                  result_code_string;
	int                        result_code;
	krb5_get_init_creds_opt    opts;
	krb5_creds                 creds;
	krb5_principal             princ;
	char                      *name;
	gic_data                   gd;

	*text = NULL;

	name = malloc(strlen(user) + strlen(realm) + 2);
	if (name == NULL) {
		*text = "Failed to allocate memory while changing password";
		return 1;
	}
	sprintf(name, "%s@%s", user, realm);

	ret = krb5_parse_name(context, name, &princ);
	free(name);
	if (ret) {
		*text = "while parsing name";
		return ret;
	}

	krb5_get_init_creds_opt_init(&opts);
	krb5_get_init_creds_opt_set_tkt_life(&opts, 5*60);
	krb5_get_init_creds_opt_set_renew_life(&opts, 0);
	krb5_get_init_creds_opt_set_forwardable(&opts, 0);
	krb5_get_init_creds_opt_set_proxiable(&opts, 0);

	gd.hinstance = hinstance;
	gd.hwnd = hwnd;
	gd.id = ID_VARDLG;

	ret = krb5_get_init_creds_password(context, &creds, princ, opasswd, gic_prompter,
		&gd, 0, "kadmin/changepw", &opts);
	if (ret) {
		*text = "while getting creds";
		return ret;
	}

	ret = krb5_change_password(context, &creds, npasswd, &result_code, &result_code_string,
		&result_string);
	if (ret) {
		*text = "while changing password";
		return ret;
	}

	if (result_code) {
		*text = malloc(result_code_string.length + result_string.length + 3);
		if (*text == NULL)
			return -1;

		sprintf(*text, "%.*s%s%.*s",
			result_code_string.length, result_code_string.data,
			(result_string.length ? ": " : ""),
			result_string.length,
			result_string.data ? result_string.data : "");
	}

	return 0;
}
