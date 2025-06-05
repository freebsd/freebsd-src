/*
 * Kerberos compatibility functions for AIX's NAS libraries.
 *
 * AIX for some reason doesn't provide the krb5_appdefault_* functions, but
 * does provide the underlying profile library functions (as a separate
 * libk5profile with a separate k5profile.h header file).
 *
 * This file is therefore (apart from the includes, opening and closing
 * comments, and the spots marked with an rra-c-util comment) a verbatim copy
 * of src/lib/krb5/krb/appdefault.c from MIT Kerberos 1.4.4.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Copyright 1985-2005 by the Massachusetts Institute of Technology.
 * For license information, see the end of this file.
 */

#include <config.h>

#include <krb5.h>
#ifdef HAVE_K5PROFILE_H
# include <k5profile.h>
#endif
#ifdef HAVE_PROFILE_H
# include <profile.h>
#endif
#include <stdio.h>
#include <string.h>

 /*xxx Duplicating this is annoying; try to work on a better way.*/
static const char *const conf_yes[] = {
	"y", "yes", "true", "t", "1", "on",
	0,
};

static const char *const conf_no[] = {
	"n", "no", "false", "nil", "0", "off",
	0,
};

static int conf_boolean(char *s)
{
	const char * const *p;
	for(p=conf_yes; *p; p++) {
		if (!strcasecmp(*p,s))
			return 1;
	}
	for(p=conf_no; *p; p++) {
		if (!strcasecmp(*p,s))
		return 0;
	}
	/* Default to "no" */
	return 0;
}

static krb5_error_code appdefault_get(krb5_context context, const char *appname, const krb5_data *realm, const char *option, char **ret_value)
{
        profile_t profile;
        const char *names[5];
	char **nameval = NULL;
	krb5_error_code retval;
	const char * realmstr =  realm?realm->data:NULL;

        /*
         * rra-c-util: The magic values are internal, so a magic check for the
         * context struct was removed here.  Call krb5_get_profile if it's
         * available since the krb5_context struct may be opaque.
         */
	    if (!context) 
	    return KV5M_CONTEXT;

#ifdef HAVE_KRB5_GET_PROFILE
            krb5_get_profile(context, &profile);
#else
	    profile = context->profile;
#endif
	    
	/*
	 * Try number one:
	 *
	 * [appdefaults]
	 *	app = {
	 *		SOME.REALM = {
	 *			option = <boolean>
	 *		}
	 *	}
	 */

	names[0] = "appdefaults";
	names[1] = appname;

	if (realmstr) {
		names[2] = realmstr;
		names[3] = option;
		names[4] = 0;
		retval = profile_get_values(profile, names, &nameval);
		if (retval == 0 && nameval && nameval[0]) {
			*ret_value = strdup(nameval[0]);
			goto goodbye;
		}
	}

	/*
	 * Try number two:
	 *
	 * [appdefaults]
	 *	app = {
	 *		option = <boolean>
	 *      }
	 */

	names[2] = option;
	names[3] = 0;
	retval = profile_get_values(profile, names, &nameval);
	if (retval == 0 && nameval && nameval[0]) {
		*ret_value = strdup(nameval[0]);
		goto goodbye;
	}

	/*
	 * Try number three:
	 *
	 * [appdefaults]
	 *	realm = {
	 *		option = <boolean>
	 */
	
	if (realmstr) {
		names[1] = realmstr;
		names[2] = option;
		names[3] = 0;
		retval = profile_get_values(profile, names, &nameval);
		if (retval == 0 && nameval && nameval[0]) {
			*ret_value = strdup(nameval[0]);
			goto goodbye;
		}
	}

	/*
	 * Try number four:
	 *
	 * [appdefaults]
	 *	option = <boolean>
	 */

	names[1] = option;
	names[2] = 0;
	retval = profile_get_values(profile, names, &nameval);
	if (retval == 0 && nameval && nameval[0]) {
		*ret_value = strdup(nameval[0]);
	} else {
		return retval;
	}

goodbye:
	if (nameval) {
		char **cpp;
		for (cpp = nameval; *cpp; cpp++)
			free(*cpp);
		free(nameval);
	}
	return 0;
}

void KRB5_CALLCONV 
krb5_appdefault_boolean(krb5_context context, const char *appname, const krb5_data *realm, const char *option, int default_value, int *ret_value)
{
	char *string = NULL;
	krb5_error_code retval;

	retval = appdefault_get(context, appname, realm, option, &string);

	if (! retval && string) {
		*ret_value = conf_boolean(string);
		free(string);
	} else
		*ret_value = default_value;
}

void KRB5_CALLCONV 
krb5_appdefault_string(krb5_context context, const char *appname, const krb5_data *realm, const char *option, const char *default_value, char **ret_value)
{
	krb5_error_code retval;
	char *string;

	retval = appdefault_get(context, appname, realm, option, &string);

	if (! retval && string) {
		*ret_value = string;
	} else {
		*ret_value = strdup(default_value);
	}
}

/*
 * Copyright (C) 1985-2005 by the Massachusetts Institute of Technology.
 * All rights reserved.
 * 
 * Export of this software from the United States of America may require
 * a specific license from the United States Government.  It is the
 * responsibility of any person or organization contemplating export to
 * obtain such a license before exporting.
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
 * fashion that it might be confused with the original MIT software.
 * M.I.T. makes no representations about the suitability of this software
 * for any purpose.  It is provided "as is" without express or implied
 * warranty.
 * 
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 * 
 * Individual source code files are copyright MIT, Cygnus Support,
 * OpenVision, Oracle, Sun Soft, FundsXpress, and others.
 * 
 * Project Athena, Athena, Athena MUSE, Discuss, Hesiod, Kerberos, Moira,
 * and Zephyr are trademarks of the Massachusetts Institute of Technology
 * (MIT).  No commercial use of these trademarks may be made without
 * prior written permission of MIT.
 * 
 * "Commercial use" means use of a name in a product or other for-profit
 * manner.  It does NOT prevent a commercial firm from referring to the
 * MIT trademarks in order to convey information (although in doing so,
 * recognition of their trademark status should be given).
 *
 * There is no SPDX-License-Identifier registered for this license.
 */
