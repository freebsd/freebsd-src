/*-
 * Copyright (c) 2002 Networks Associates Technologies, Inc.
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project by ThinkSec AS and
 * NAI Labs, the Security Research Division of Network Associates, Inc.
 * under DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"), as part of the
 * DARPA CHATS research program.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
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
 * $Id$
 */

#include <sys/param.h>
#include <sys/wait.h>

#include <err.h>
#include <pwd.h>
#include <stdio.h>
#include <syslog.h>
#include <unistd.h>

#include <security/pam_appl.h>
#include <security/openpam.h>

static pam_handle_t *pamh;
static struct pam_conv pamc;

static void
usage(void)
{

	fprintf(stderr, "Usage: su [login [args]]\n");
	exit(1);
}

static int
check(const char *func, int pam_err)
{

	if (pam_err == PAM_SUCCESS || pam_err == PAM_NEW_AUTHTOK_REQD)
		return pam_err;
	openlog("su", LOG_CONS, LOG_AUTH);
	syslog(LOG_ERR, "%s(): %s", func, pam_strerror(pamh, pam_err));
	errx(1, "Sorry.");
}

int
main(int argc, char *argv[])
{
	char hostname[MAXHOSTNAMELEN];
	const char *user, *tty;
	struct passwd *pwd;
	int o, status;
	pid_t pid;

	while ((o = getopt(argc, argv, "h")) != -1)
		switch (o) {
		case 'h':
		default:
			usage();
		}

	argc -= optind;
	argv += optind;

	/* initialize PAM */
	pamc.conv = &openpam_ttyconv;
	pam_start("su", argc ? *argv : "root", &pamc, &pamh);

	/* set some items */
	gethostname(hostname, sizeof(hostname));
	check("pam_set_item", pam_set_item(pamh, PAM_RHOST, hostname));
	user = getlogin();
	check("pam_set_item", pam_set_item(pamh, PAM_RUSER, user));
	tty = ttyname(STDERR_FILENO);
	check("pam_set_item", pam_set_item(pamh, PAM_TTY, tty));

	/* authenticate the applicant */
	check("pam_authenticate", pam_authenticate(pamh, 0));
	if (check("pam_acct_mgmt", pam_acct_mgmt(pamh, 0)) ==
	    PAM_NEW_AUTHTOK_REQD)
		check("pam_chauthtok",
		    pam_chauthtok(pamh, PAM_CHANGE_EXPIRED_AUTHTOK));

	/* establish the requested credentials */
	check("pam_setcred", pam_setcred(pamh, PAM_ESTABLISH_CRED));

	/* authentication succeeded; open a session */
	check("pam_open_session", pam_open_session(pamh, 0));

	if (initgroups(pwd->pw_name, pwd->pw_gid) == -1)
		err(1, "initgroups()");
	if (setuid(pwd->pw_uid) == -1)
		err(1, "setuid()");

	/* XXX export environment variables */

	switch ((pid = fork())) {
	case -1:
		err(1, "fork()");
	case 0:
		/* child: start a shell */
		*argv = pwd->pw_shell;
		execvp(*argv, argv);
		err(1, "execvp()");
	default:
		/* parent: wait for child to exit */
		waitpid(pid, &status, 0);
		if (WIFEXITED(status))
			status = WEXITSTATUS(status);
		else
			status = 1;
	}

	/* close the session and release PAM resources */
	check("pam_close_session", pam_close_session(pamh, 0));
	check("pam_end", pam_end(pamh, 0));

	exit(status);
}
