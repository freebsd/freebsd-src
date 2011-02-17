/*
 * Copyright (c) 1997 - 2001 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden). 
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 *
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright 
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the distribution. 
 *
 * 3. Neither the name of the Institute nor the names of its contributors 
 *    may be used to endorse or promote products derived from this software 
 *    without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE 
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS 
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE. 
 */

#include "krb5_locl.h"

RCSID("$Id: get_default_principal.c 14870 2005-04-20 20:53:29Z lha $");

/*
 * Try to find out what's a reasonable default principal.
 */

static const char*
get_env_user(void)
{
    const char *user = getenv("USER");
    if(user == NULL)
	user = getenv("LOGNAME");
    if(user == NULL)
	user = getenv("USERNAME");
    return user;
}

/*
 * Will only use operating-system dependant operation to get the
 * default principal, for use of functions that in ccache layer to
 * avoid recursive calls.
 */

krb5_error_code
_krb5_get_default_principal_local (krb5_context context, 
				   krb5_principal *princ)
{
    krb5_error_code ret;
    const char *user;
    uid_t uid;

    *princ = NULL;

    uid = getuid();    
    if(uid == 0) {
	user = getlogin();
	if(user == NULL)
	    user = get_env_user();
	if(user != NULL && strcmp(user, "root") != 0)
	    ret = krb5_make_principal(context, princ, NULL, user, "root", NULL);
	else
	    ret = krb5_make_principal(context, princ, NULL, "root", NULL);
    } else {
	struct passwd *pw = getpwuid(uid);	
	if(pw != NULL)
	    user = pw->pw_name;
	else {
	    user = get_env_user();
	    if(user == NULL)
		user = getlogin();
	}
	if(user == NULL) {
	    krb5_set_error_string(context,
				  "unable to figure out current principal");
	    return ENOTTY; /* XXX */
	}
	ret = krb5_make_principal(context, princ, NULL, user, NULL);
    }
    return ret;
}

krb5_error_code KRB5_LIB_FUNCTION
krb5_get_default_principal (krb5_context context,
			    krb5_principal *princ)
{
    krb5_error_code ret;
    krb5_ccache id;

    *princ = NULL;

    ret = krb5_cc_default (context, &id);
    if (ret == 0) {
	ret = krb5_cc_get_principal (context, id, princ);
	krb5_cc_close (context, id);
	if (ret == 0)
	    return 0;
    }

    return _krb5_get_default_principal_local(context, princ);
}
