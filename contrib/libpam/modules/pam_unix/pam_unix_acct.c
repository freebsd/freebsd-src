/*
 * Copyright Elliot Lee, 1996.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, and the entire permission notice in its entirety,
 *    including the disclaimer of warranties.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 * 
 * ALTERNATIVELY, this product may be distributed under the terms of
 * the GNU Public License, in which case the provisions of the GPL are
 * required INSTEAD OF the above restrictions.  (This clause is
 * necessary due to a potential bad interaction between the GPL and
 * the restrictions contained in a BSD-style copyright.)
 * 
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* pam_unix_acct.c module, different track */

#ifdef linux
# define _GNU_SOURCE
# include <features.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define __USE_MISC
#include <pwd.h>
#include <sys/types.h>
#include <syslog.h>
#include <unistd.h>
#ifdef HAVE_SHADOW_H
#include <shadow.h>
#endif
#include <time.h>

#define PAM_SM_ACCOUNT

#ifndef LINUX
# include <security/pam_appl.h>
#endif

#define _PAM_EXTERN_FUNCTIONS
#include <security/pam_modules.h>

PAM_EXTERN
int pam_sm_acct_mgmt(pam_handle_t *pamh, int flags,
		     int argc, const char **argv)
{
#ifdef HAVE_SHADOW_H
  const char *uname;
  int retval;
  time_t curdays;
  struct spwd *spent;
  struct passwd *pwent;

  setpwent();
  setspent();
  retval = pam_get_item(pamh,PAM_USER,(const void **)&uname);
  if(retval != PAM_SUCCESS || uname == NULL) {
    return PAM_SUCCESS; /* Couldn't get username, just ignore this
			(i.e. they don't have any expiry info available */
  }
  pwent = getpwnam(uname);
  if(!pwent)
    return PAM_USER_UNKNOWN;
  if(strcmp(pwent->pw_passwd,"x"))
    return PAM_SUCCESS; /* They aren't using shadow passwords & expiry
			   info */
  spent = getspnam(uname);
  if(!spent)
    return PAM_SUCCESS; /* Couldn't get username from shadow, just ignore this
			(i.e. they don't have any expiry info available */
  curdays = time(NULL)/(60*60*24);
  if((curdays > (spent->sp_lstchg + spent->sp_max + spent->sp_inact))
	&& (spent->sp_max != -1) && (spent->sp_inact != -1))
	return PAM_ACCT_EXPIRED;
  if((curdays > spent->sp_expire) && (spent->sp_expire != -1))
	return PAM_ACCT_EXPIRED;
  endspent();
  endpwent();
#endif
    return PAM_SUCCESS;
}


/* static module data */
#ifdef PAM_STATIC
struct pam_module _pam_unix_acct_modstruct = {
    "pam_unix_acct",
    NULL,
    NULL,
    pam_sm_acct_mgmt,
    NULL,
    NULL,
    NULL,
};
#endif
