/* Copyright 1988,1990,1993,1994 by Paul Vixie
 * All rights reserved
 */

/*
 * Copyright (c) 1997 by Internet Software Consortium
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

#if !defined(lint) && !defined(LINT)
static const char rcsid[] =
    "$Id: do_command.c,v 1.3 1998/08/14 00:32:39 vixie Exp $";
#endif

#include "cron.h"
#if defined(LOGIN_CAP)
# include <login_cap.h>
#endif
#ifdef PAM
# include <security/pam_appl.h>
# include <security/openpam.h>
#endif

static void		child_process(entry *, user *);
static WAIT_T		wait_on_child(PID_T, const char *);

extern char	*environ;

void
do_command(entry *e, user *u)
{
	pid_t pid;

	Debug(DPROC, ("[%d] do_command(%s, (%s,%d,%d))\n",
		getpid(), e->cmd, u->name, e->uid, e->gid))

	/* fork to become asynchronous -- parent process is done immediately,
	 * and continues to run the normal cron code, which means return to
	 * tick().  the child and grandchild don't leave this function, alive.
	 */
	switch ((pid = fork())) {
	case -1:
		log_it("CRON", getpid(), "error", "can't fork");
		if (e->flags & INTERVAL)
			e->lastexit = time(NULL);
		break;
	case 0:
		/* child process */
		pidfile_close(pfh);
		child_process(e, u);
		Debug(DPROC, ("[%d] child process done, exiting\n", getpid()))
		_exit(OK_EXIT);
		break;
	default:
		/* parent process */
		Debug(DPROC, ("[%d] main process forked child #%d, "
		    "returning to work\n", getpid(), pid))
		if (e->flags & INTERVAL) {
			e->lastexit = 0;
			e->child = pid;
		}
		break;
	}
	Debug(DPROC, ("[%d] main process returning to work\n", getpid()))
}


static void
child_process(entry *e, user *u)
{
	int stdin_pipe[2], stdout_pipe[2];
	char *input_data;
	const char *usernm, *mailto, *mailfrom;
	PID_T jobpid, stdinjob, mailpid;
	FILE *mail;
	int bytes = 1;
	int status = 0;
	const char *homedir = NULL;
# if defined(LOGIN_CAP)
	struct passwd *pwd;
	login_cap_t *lc;
# endif

	Debug(DPROC, ("[%d] child_process('%s')\n", getpid(), e->cmd))

	/* mark ourselves as different to PS command watchers by upshifting
	 * our program name.  This has no effect on some kernels.
	 */
	setproctitle("running job");

	/* discover some useful and important environment settings
	 */
	usernm = env_get("LOGNAME", e->envp);
	mailto = env_get("MAILTO", e->envp);
	mailfrom = env_get("MAILFROM", e->envp);

#ifdef PAM
	/* use PAM to see if the user's account is available,
	 * i.e., not locked or expired or whatever.  skip this
	 * for system tasks from /etc/crontab -- they can run
	 * as any user.
	 */
	if (strcmp(u->name, SYS_NAME)) {	/* not equal */
		pam_handle_t *pamh = NULL;
		int pam_err;
		struct pam_conv pamc = {
			.conv = openpam_nullconv,
			.appdata_ptr = NULL
		};

		Debug(DPROC, ("[%d] checking account with PAM\n", getpid()))

		/* u->name keeps crontab owner name while LOGNAME is the name
		 * of user to run command on behalf of.  they should be the
		 * same for a task from a per-user crontab.
		 */
		if (strcmp(u->name, usernm)) {
			log_it(usernm, getpid(), "username ambiguity", u->name);
			exit(ERROR_EXIT);
		}

		pam_err = pam_start("cron", usernm, &pamc, &pamh);
		if (pam_err != PAM_SUCCESS) {
			log_it("CRON", getpid(), "error", "can't start PAM");
			exit(ERROR_EXIT);
		}

		pam_err = pam_acct_mgmt(pamh, PAM_SILENT);
		/* Expired password shouldn't prevent the job from running. */
		if (pam_err != PAM_SUCCESS && pam_err != PAM_NEW_AUTHTOK_REQD) {
			log_it(usernm, getpid(), "USER", "account unavailable");
			exit(ERROR_EXIT);
		}

		pam_end(pamh, pam_err);
	}
#endif

	/* our parent is watching for our death by catching SIGCHLD.  we
	 * do not care to watch for our children's deaths this way -- we
	 * use wait() explicitly.  so we have to disable the signal (which
	 * was inherited from the parent).
	 */
	(void) signal(SIGCHLD, SIG_DFL);

	/* create some pipes to talk to our future child
	 */
	if (pipe(stdin_pipe) != 0 || pipe(stdout_pipe) != 0) {
		log_it("CRON", getpid(), "error", "can't pipe");
		exit(ERROR_EXIT);
	}

	/* since we are a forked process, we can diddle the command string
	 * we were passed -- nobody else is going to use it again, right?
	 *
	 * if a % is present in the command, previous characters are the
	 * command, and subsequent characters are the additional input to
	 * the command.  Subsequent %'s will be transformed into newlines,
	 * but that happens later.
	 *
	 * If there are escaped %'s, remove the escape character.
	 */
	/*local*/{
		int escaped = FALSE;
		int ch;
		char *p;

		for (input_data = p = e->cmd;
		     (ch = *input_data) != '\0';
		     input_data++, p++) {
			if (p != input_data)
				*p = ch;
			if (escaped) {
				if (ch == '%' || ch == '\\')
					*--p = ch;
				escaped = FALSE;
				continue;
			}
			if (ch == '\\') {
				escaped = TRUE;
				continue;
			}
			if (ch == '%') {
				*input_data++ = '\0';
				break;
			}
		}
		*p = '\0';
	}

	/* fork again, this time so we can exec the user's command.
	 */
	switch (jobpid = fork()) {
	case -1:
		log_it("CRON", getpid(), "error", "can't fork");
		exit(ERROR_EXIT);
		/*NOTREACHED*/
	case 0:
		Debug(DPROC, ("[%d] grandchild process fork()'ed\n",
			      getpid()))

		if (e->uid == ROOT_UID)
			Jitter = RootJitter;
		if (Jitter != 0) {
			srandom(getpid());
			sleep(random() % Jitter);
		}

		/* write a log message.  we've waited this long to do it
		 * because it was not until now that we knew the PID that
		 * the actual user command shell was going to get and the
		 * PID is part of the log message.
		 */
		if ((e->flags & DONT_LOG) == 0) {
			char *x = mkprints((u_char *)e->cmd, strlen(e->cmd));

			log_it(usernm, getpid(), "CMD", x);
			free(x);
		}

		/* that's the last thing we'll log.  close the log files.
		 */
#ifdef SYSLOG
		closelog();
#endif

		/* get new pgrp, void tty, etc.
		 */
		(void) setsid();

		/* close the pipe ends that we won't use.  this doesn't affect
		 * the parent, who has to read and write them; it keeps the
		 * kernel from recording us as a potential client TWICE --
		 * which would keep it from sending SIGPIPE in otherwise
		 * appropriate circumstances.
		 */
		close(stdin_pipe[WRITE_PIPE]);
		close(stdout_pipe[READ_PIPE]);

		/* grandchild process.  make std{in,out} be the ends of
		 * pipes opened by our daddy; make stderr go to stdout.
		 */
		close(STDIN);	dup2(stdin_pipe[READ_PIPE], STDIN);
		close(STDOUT);	dup2(stdout_pipe[WRITE_PIPE], STDOUT);
		close(STDERR);	dup2(STDOUT, STDERR);

		/* close the pipes we just dup'ed.  The resources will remain.
		 */
		close(stdin_pipe[READ_PIPE]);
		close(stdout_pipe[WRITE_PIPE]);

		environ = NULL;

# if defined(LOGIN_CAP)
		/* Set user's entire context, but note that PATH will
		 * be overridden later
		 */
		if ((pwd = getpwnam(usernm)) == NULL)
			pwd = getpwuid(e->uid);
		lc = NULL;
		if (pwd != NULL) {
			if (pwd->pw_dir != NULL
			    && pwd->pw_dir[0] != '\0') {
				homedir = strdup(pwd->pw_dir);
				if (homedir == NULL) {
					warn("strdup");
					_exit(ERROR_EXIT);
				}
			}
			pwd->pw_gid = e->gid;
			if (e->class != NULL)
				lc = login_getclass(e->class);
		}
		if (pwd &&
		    setusercontext(lc, pwd, e->uid,
			    LOGIN_SETALL) == 0)
			(void) endpwent();
		else {
			/* fall back to the old method */
			(void) endpwent();
# endif
			/* set our directory, uid and gid.  Set gid first,
			 * since once we set uid, we've lost root privileges.
			 */
			if (setgid(e->gid) != 0) {
				log_it(usernm, getpid(),
				    "error", "setgid failed");
				_exit(ERROR_EXIT);
			}
			if (initgroups(usernm, e->gid) != 0) {
				log_it(usernm, getpid(),
				    "error", "initgroups failed");
				_exit(ERROR_EXIT);
			}
			if (setlogin(usernm) != 0) {
				log_it(usernm, getpid(),
				    "error", "setlogin failed");
				_exit(ERROR_EXIT);
			}
			if (setuid(e->uid) != 0) {
				log_it(usernm, getpid(),
				    "error", "setuid failed");
				_exit(ERROR_EXIT);
			}
			/* we aren't root after this..*/
#if defined(LOGIN_CAP)
		}
		if (lc != NULL)
			login_close(lc);
#endif

		/* For compatibility, we chdir to the value of HOME if it was
		 * specified explicitly in the crontab file, but not if it was
		 * set in the environment by some other mechanism. We chdir to
		 * the homedir given by the pw entry otherwise.
		 *
		 * If !LOGIN_CAP, then HOME is always set in e->envp.
		 *
		 * XXX: probably should also consult PAM.
		 */
		{
			char	*new_home = env_get("HOME", e->envp);
			if (new_home != NULL && new_home[0] != '\0')
				chdir(new_home);
			else if (homedir != NULL)
				chdir(homedir);
			else
				chdir("/");
		}

		/* exec the command. Note that SHELL is not respected from
		 * either login.conf or pw_shell, only an explicit setting
		 * in the crontab. (default of _PATH_BSHELL is supplied when
		 * setting up the entry)
		 */
		{
			char	*shell = env_get("SHELL", e->envp);
			char	**p;

			/* Apply the environment from the entry, overriding
			 * existing values (this will always set LOGNAME and
			 * SHELL). putenv should not fail unless malloc does.
			 */
			for (p = e->envp; *p; ++p) {
				if (putenv(*p) != 0) {
					warn("putenv");
					_exit(ERROR_EXIT);
				}
			}

			/* HOME in login.conf overrides pw, and HOME in the
			 * crontab overrides both. So set pw's value only if
			 * nothing was already set (overwrite==0).
			 */
			if (homedir != NULL
			    && setenv("HOME", homedir, 0) < 0) {
				warn("setenv(HOME)");
				_exit(ERROR_EXIT);
			}

			/* PATH in login.conf is respected, but the crontab
			 * overrides; set a default value only if nothing
			 * already set.
			 */
			if (setenv("PATH", _PATH_DEFPATH, 0) < 0) {
				warn("setenv(PATH)");
				_exit(ERROR_EXIT);
			}

# if DEBUGGING
			if (DebugFlags & DTEST) {
				fprintf(stderr,
				"debug DTEST is on, not exec'ing command.\n");
				fprintf(stderr,
				"\tcmd='%s' shell='%s'\n", e->cmd, shell);
				_exit(OK_EXIT);
			}
# endif /*DEBUGGING*/
			execl(shell, shell, "-c", e->cmd, (char *)NULL);
			warn("execl: couldn't exec `%s'", shell);
			_exit(ERROR_EXIT);
		}
		break;
	default:
		/* parent process */
		break;
	}

	/* middle process, child of original cron, parent of process running
	 * the user's command.
	 */

	Debug(DPROC, ("[%d] child continues, closing pipes\n", getpid()))

	/* close the ends of the pipe that will only be referenced in the
	 * grandchild process...
	 */
	close(stdin_pipe[READ_PIPE]);
	close(stdout_pipe[WRITE_PIPE]);

	/*
	 * write, to the pipe connected to child's stdin, any input specified
	 * after a % in the crontab entry.  while we copy, convert any
	 * additional %'s to newlines.  when done, if some characters were
	 * written and the last one wasn't a newline, write a newline.
	 *
	 * Note that if the input data won't fit into one pipe buffer (2K
	 * or 4K on most BSD systems), and the child doesn't read its stdin,
	 * we would block here.  thus we must fork again.
	 */

	if (*input_data && (stdinjob = fork()) == 0) {
		FILE *out = fdopen(stdin_pipe[WRITE_PIPE], "w");
		int need_newline = FALSE;
		int escaped = FALSE;
		int ch;

		if (out == NULL) {
			warn("fdopen failed in child2");
			_exit(ERROR_EXIT);
		}

		Debug(DPROC, ("[%d] child2 sending data to grandchild\n", getpid()))

		/* close the pipe we don't use, since we inherited it and
		 * are part of its reference count now.
		 */
		close(stdout_pipe[READ_PIPE]);

		/* translation:
		 *	\% -> %
		 *	%  -> \n
		 *	\x -> \x	for all x != %
		 */
		while ((ch = *input_data++) != '\0') {
			if (escaped) {
				if (ch != '%')
					putc('\\', out);
			} else {
				if (ch == '%')
					ch = '\n';
			}

			if (!(escaped = (ch == '\\'))) {
				putc(ch, out);
				need_newline = (ch != '\n');
			}
		}
		if (escaped)
			putc('\\', out);
		if (need_newline)
			putc('\n', out);

		/* close the pipe, causing an EOF condition.  fclose causes
		 * stdin_pipe[WRITE_PIPE] to be closed, too.
		 */
		fclose(out);

		Debug(DPROC, ("[%d] child2 done sending to grandchild\n", getpid()))
		exit(0);
	}

	/* close the pipe to the grandkiddie's stdin, since its wicked uncle
	 * ernie back there has it open and will close it when he's done.
	 */
	close(stdin_pipe[WRITE_PIPE]);

	/*
	 * read output from the grandchild.  it's stderr has been redirected to
	 * it's stdout, which has been redirected to our pipe.  if there is any
	 * output, we'll be mailing it to the user whose crontab this is...
	 * when the grandchild exits, we'll get EOF.
	 */

	Debug(DPROC, ("[%d] child reading output from grandchild\n", getpid()))

	/*local*/{
		FILE *in = fdopen(stdout_pipe[READ_PIPE], "r");
		int ch;

		if (in == NULL) {
			warn("fdopen failed in child");
			_exit(ERROR_EXIT);
		}

		mail = NULL;

		ch = getc(in);
		if (ch != EOF) {
			Debug(DPROC|DEXT,
				("[%d] got data (%x:%c) from grandchild\n",
					getpid(), ch, ch))

			/* get name of recipient.  this is MAILTO if set to a
			 * valid local username; USER otherwise.
			 */
			if (mailto == NULL) {
				/* MAILTO not present, set to USER,
				 * unless globally overridden.
				 */
				if (defmailto)
					mailto = defmailto;
				else
					mailto = usernm;
			}
			if (mailto && *mailto == '\0')
				mailto = NULL;

			/* if we are supposed to be mailing, MAILTO will
			 * be non-NULL.  only in this case should we set
			 * up the mail command and subjects and stuff...
			 */

			if (mailto) {
				char	**env;
				char	mailcmd[MAX_COMMAND];
				char	hostname[MAXHOSTNAMELEN];

				if (gethostname(hostname, MAXHOSTNAMELEN) == -1)
					hostname[0] = '\0';
				hostname[sizeof(hostname) - 1] = '\0';
				if (snprintf(mailcmd, sizeof(mailcmd), MAILFMT,
				    MAILARG) >= sizeof(mailcmd)) {
					warnx("mail command too long");
					(void) _exit(ERROR_EXIT);
				}
				if (!(mail = cron_popen(mailcmd, "w", e, &mailpid))) {
					warn("%s", mailcmd);
					(void) _exit(ERROR_EXIT);
				}
				if (mailfrom == NULL || *mailfrom == '\0')
					fprintf(mail, "From: Cron Daemon <%s@%s>\n",
					    usernm, hostname);
				else
					fprintf(mail, "From: Cron Daemon <%s>\n",
					    mailfrom);
				fprintf(mail, "To: %s\n", mailto);
				fprintf(mail, "Subject: Cron <%s@%s> %s\n",
					usernm, first_word(hostname, "."),
					e->cmd);
#ifdef MAIL_DATE
				fprintf(mail, "Date: %s\n",
					arpadate(&TargetTime));
#endif /*MAIL_DATE*/
				for (env = e->envp;  *env;  env++)
					fprintf(mail, "X-Cron-Env: <%s>\n",
						*env);
				fprintf(mail, "\n");

				/* this was the first char from the pipe
				 */
				putc(ch, mail);
			}

			/* we have to read the input pipe no matter whether
			 * we mail or not, but obviously we only write to
			 * mail pipe if we ARE mailing.
			 */

			while (EOF != (ch = getc(in))) {
				bytes++;
				if (mail)
					putc(ch, mail);
			}
		}
		/*if data from grandchild*/

		Debug(DPROC, ("[%d] got EOF from grandchild\n", getpid()))

		/* also closes stdout_pipe[READ_PIPE] */
		fclose(in);
	}

	/* wait for children to die.
	 */
	if (jobpid > 0) {
		WAIT_T waiter;

		waiter = wait_on_child(jobpid, "grandchild command job");

		/* If everything went well, and -n was set, _and_ we have mail,
		 * we won't be mailing... so shoot the messenger!
		 */
		if (WIFEXITED(waiter) && WEXITSTATUS(waiter) == 0
		    && (e->flags & MAIL_WHEN_ERR) == MAIL_WHEN_ERR
		    && mail) {
			Debug(DPROC, ("[%d] %s executed successfully, mail suppressed\n",
				getpid(), "grandchild command job"))
			kill(mailpid, SIGKILL);
			(void)fclose(mail);
			mail = NULL;
		}

		/* only close pipe if we opened it -- i.e., we're
		 * mailing...
		 */

		if (mail) {
			Debug(DPROC, ("[%d] closing pipe to mail\n",
				getpid()))
			/* Note: the pclose will probably see
			 * the termination of the grandchild
			 * in addition to the mail process, since
			 * it (the grandchild) is likely to exit
			 * after closing its stdout.
			 */
			status = cron_pclose(mail);

			/* if there was output and we could not mail it,
			 * log the facts so the poor user can figure out
			 * what's going on.
			 */
			if (status) {
				char buf[MAX_TEMPSTR];

				snprintf(buf, sizeof(buf),
			"mailed %d byte%s of output but got status 0x%04x\n",
					bytes, (bytes==1)?"":"s",
					status);
				log_it(usernm, getpid(), "MAIL", buf);
			}
		}
	}

	if (*input_data && stdinjob > 0)
		wait_on_child(stdinjob, "grandchild stdinjob");
}

static WAIT_T
wait_on_child(PID_T childpid, const char *name)
{
	WAIT_T waiter;
	PID_T pid;

	Debug(DPROC, ("[%d] waiting for %s (%d) to finish\n",
		getpid(), name, childpid))

#ifdef POSIX
	while ((pid = waitpid(childpid, &waiter, 0)) < 0 && errno == EINTR)
#else
	while ((pid = wait4(childpid, &waiter, 0, NULL)) < 0 && errno == EINTR)
#endif
		;

	if (pid < OK)
		return waiter;

	Debug(DPROC, ("[%d] %s (%d) finished, status=%04x",
		getpid(), name, pid, WEXITSTATUS(waiter)))
	if (WIFSIGNALED(waiter) && WCOREDUMP(waiter))
		Debug(DPROC, (", dumped core"))
	Debug(DPROC, ("\n"))

	return waiter;
}
