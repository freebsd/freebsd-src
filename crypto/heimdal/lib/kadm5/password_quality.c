/*
 * Copyright (c) 1997-1999 Kungliga Tekniska Högskolan
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

#include "kadm5_locl.h"

RCSID("$Id: password_quality.c,v 1.4 2000/07/05 13:14:45 joda Exp $");

#ifdef HAVE_DLFCN_H
#include <dlfcn.h>
#endif

static const char *
simple_passwd_quality (krb5_context context,
		       krb5_principal principal,
		       krb5_data *pwd)
{
    if (pwd->length < 6)
	return "Password too short";
    else
	return NULL;
}

typedef const char* (*passwd_quality_check_func)(krb5_context, 
						 krb5_principal, 
						 krb5_data*);

static passwd_quality_check_func passwd_quality_check = simple_passwd_quality;

#ifdef HAVE_DLOPEN

#define PASSWD_VERSION 0

#endif

/*
 * setup the password quality hook
 */

void
kadm5_setup_passwd_quality_check(krb5_context context,
				 const char *check_library,
				 const char *check_function)
{
#ifdef HAVE_DLOPEN
    void *handle;
    void *sym;
    int *version;
    int flags;
    const char *tmp;

#ifdef RTLD_NOW
    flags = RTLD_NOW;
#else
    flags = 0;
#endif

    if(check_library == NULL) {
	tmp = krb5_config_get_string(context, NULL, 
				     "password_quality", 
				     "check_library", 
				     NULL);
	if(tmp != NULL)
	    check_library = tmp;
    }
    if(check_function == NULL) {
	tmp = krb5_config_get_string(context, NULL, 
				     "password_quality", 
				     "check_function", 
				     NULL);
	if(tmp != NULL)
	    check_function = tmp;
    }
    if(check_library != NULL && check_function == NULL)
	check_function = "passwd_check";

    if(check_library == NULL)
	return;
    handle = dlopen(check_library, flags);
    if(handle == NULL) {
	krb5_warnx(context, "failed to open `%s'", check_library);
	return;
    }
    version = dlsym(handle, "version");
    if(version == NULL) {
	krb5_warnx(context,
		   "didn't find `version' symbol in `%s'", check_library);
	dlclose(handle);
	return;
    }
    if(*version != PASSWD_VERSION) {
	krb5_warnx(context,
		   "version of loaded library is %d (expected %d)",
		   *version, PASSWD_VERSION);
	dlclose(handle);
	return;
    }
    sym = dlsym(handle, check_function);
    if(sym == NULL) {
	krb5_warnx(context, 
		   "didn't find `%s' symbol in `%s'", 
		   check_function, check_library);
	dlclose(handle);
	return;
    }
    passwd_quality_check = (passwd_quality_check_func) sym;
#endif /* HAVE_DLOPEN */
}

const char *
kadm5_check_password_quality (krb5_context context,
			      krb5_principal principal,
			      krb5_data *pwd_data)
{
    return (*passwd_quality_check) (context, principal, pwd_data);
}
