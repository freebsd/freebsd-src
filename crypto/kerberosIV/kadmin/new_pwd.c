/*
 * Copyright (c) 1995, 1996, 1997, 1998 Kungliga Tekniska Högskolan
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

#include "kadm_locl.h"

RCSID("$Id: new_pwd.c,v 1.14 1999/12/02 16:58:36 joda Exp $");

#ifdef NOENCRYPTION
#define read_long_pw_string placebo_read_pw_string
#else
#define read_long_pw_string des_read_pw_string
#endif

static char *
check_pw (char *pword)
{
    int ret = kadm_check_pw(pword);
    switch(ret) {
    case 0:
	return NULL;
    case KADM_PASS_Q_NULL:
	return "Null passwords are not allowed - "
	    "Please enter a longer password.";
    case KADM_PASS_Q_TOOSHORT:
	return "Password is to short - Please enter a longer password.";
    case KADM_PASS_Q_CLASS:
	/* XXX */
	return "Please don't use an all-lower case password.\n"
	    "\tUnusual capitalization, delimiter characters or "
	    "digits are suggested.";
    }
    return "Password is insecure"; /* XXX this shouldn't happen */
}

int
get_pw_new_pwd(char *pword, int pwlen, krb_principal *pr, int print_realm)
{
    char ppromp[40+ANAME_SZ+INST_SZ+REALM_SZ]; /* for the password prompt */
    char npromp[40+ANAME_SZ+INST_SZ+REALM_SZ]; /* for the password prompt */
    
    char p[MAX_K_NAME_SZ];
    
    char local_realm[REALM_SZ];
    int status;
    char *expl;
    
    /*
     * We don't care about failure; this is to determine whether or
     * not to print the realm in the prompt for a new password. 
     */
    krb_get_lrealm(local_realm, 1);
    
    if (strcmp(local_realm, pr->realm))
	print_realm++;
    
    {
	char *q;
	krb_unparse_name_r(pr, p);
	if(print_realm == 0 && (q = strrchr(p, '@')))
	    *q = 0;
    }

    snprintf(ppromp, sizeof(ppromp), "Old password for %s:", p);
    if (read_long_pw_string(pword, pwlen-1, ppromp, 0)) {
	fprintf(stderr, "Error reading old password.\n");
	return -1;
    }

    status = krb_get_pw_in_tkt(pr->name, pr->instance, pr->realm, 
			       PWSERV_NAME, KADM_SINST, 1, pword);
    if (status != KSUCCESS) {
	if (status == INTK_BADPW) {
	    printf("Incorrect old password.\n");
	    return -1;
	}
	else {
	    fprintf(stderr, "Kerberos error: %s\n", krb_get_err_text(status));
	    return -1;
	}
    }
    memset(pword, 0, pwlen);
    
    do {
	char verify[MAX_KPW_LEN];

	snprintf(npromp, sizeof(npromp), "New Password for %s:",p);
	if (read_long_pw_string(pword, pwlen-1, npromp, 0)) {
	    fprintf(stderr,
		    "Error reading new password, password unchanged.\n");
	    return -1;
        }
	expl = check_pw (pword);
	if (expl) {
	    printf("\n\t%s\n\n", expl);
	    continue;
	}
	/* Now we got an ok password, verify it. */
	snprintf(npromp, sizeof(npromp), "Verifying New Password for %s:", p);
	if (read_long_pw_string(verify, MAX_KPW_LEN-1, npromp, 0)) {
	    fprintf(stderr,
		    "Error reading new password, password unchanged.\n");
	    return -1;
        }
	if (strcmp(pword, verify) != 0) {
	    printf("Verify failure - try again\n");
	    expl = "";		/* continue */
	}
    } while (expl);
    return 0;
}
