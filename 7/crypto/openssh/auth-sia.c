/*
 * Copyright (c) 2002 Chris Adams.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "includes.h"

#ifdef HAVE_OSF_SIA
#include <sia.h>
#include <siad.h>
#include <pwd.h>
#include <signal.h>
#include <setjmp.h>
#include <sys/resource.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
#include <sys/security.h>
#include <prot.h>
#include <time.h>

#include "ssh.h"
#include "key.h"
#include "hostfile.h"
#include "auth.h"
#include "auth-sia.h"
#include "log.h"
#include "servconf.h"
#include "canohost.h"
#include "uidswap.h"

extern ServerOptions options;
extern int saved_argc;
extern char **saved_argv;

static int
sia_password_change_required(const char *user)
{
	struct es_passwd *acct;
	time_t pw_life;
	time_t pw_date;

	set_auth_parameters(saved_argc, saved_argv);

	if ((acct = getespwnam(user)) == NULL) {
		error("Couldn't access protected database entry for %s", user);
		endprpwent();
		return (0);
	}

	/* If forced password change flag is set, honor it */
	if (acct->uflg->fg_psw_chg_reqd && acct->ufld->fd_psw_chg_reqd) {
		endprpwent();
		return (1);
	}

	/* Obtain password lifetime; if none, it can't have expired */
	if (acct->uflg->fg_expire)
		pw_life = acct->ufld->fd_expire;
	else if (acct->sflg->fg_expire)
		pw_life = acct->sfld->fd_expire;
	else {
		endprpwent();
		return (0);
	}

	/* Offset from last change; if none, it must be expired */
	if (acct->uflg->fg_schange)
		pw_date = acct->ufld->fd_schange + pw_life;
	else {
		endprpwent();
		return (1);
	}

	endprpwent();

	/* If expiration date is prior to now, change password */
	
	return (pw_date <= time((time_t *) NULL));
}

int
sys_auth_passwd(Authctxt *authctxt, const char *pass)
{
	int ret;
	SIAENTITY *ent = NULL;
	const char *host;

	host = get_canonical_hostname(options.use_dns);

	if (!authctxt->user || pass == NULL || pass[0] == '\0')
		return (0);

	if (sia_ses_init(&ent, saved_argc, saved_argv, host, authctxt->user,
	    NULL, 0, NULL) != SIASUCCESS)
		return (0);

	if ((ret = sia_ses_authent(NULL, pass, ent)) != SIASUCCESS) {
		error("Couldn't authenticate %s from %s",
		    authctxt->user, host);
		if (ret & SIASTOP)
			sia_ses_release(&ent);

		return (0);
	}

	sia_ses_release(&ent);

	authctxt->force_pwchange = sia_password_change_required(
		authctxt->user);

	return (1);
}

void
session_setup_sia(struct passwd *pw, char *tty)
{
	SIAENTITY *ent = NULL;
	const char *host;

	host = get_canonical_hostname(options.use_dns);

	if (sia_ses_init(&ent, saved_argc, saved_argv, host, pw->pw_name,
	    tty, 0, NULL) != SIASUCCESS)
		fatal("sia_ses_init failed");

	if (sia_make_entity_pwd(pw, ent) != SIASUCCESS) {
		sia_ses_release(&ent);
		fatal("sia_make_entity_pwd failed");
	}

	ent->authtype = SIA_A_NONE;
	if (sia_ses_estab(sia_collect_trm, ent) != SIASUCCESS)
		fatal("Couldn't establish session for %s from %s",
		    pw->pw_name, host);

	if (sia_ses_launch(sia_collect_trm, ent) != SIASUCCESS)
		fatal("Couldn't launch session for %s from %s",
		    pw->pw_name, host);

	sia_ses_release(&ent);

	setuid(0);
	permanently_set_uid(pw);
}

#endif /* HAVE_OSF_SIA */
