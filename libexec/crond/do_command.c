#if !defined(lint) && !defined(LINT)
static char rcsid[] = "$Header: /a/cvs/386BSD/src/libexec/crond/do_command.c,v 1.1.1.1 1993/06/12 14:55:03 rgrimes Exp $";
#endif

/* $Source: /a/cvs/386BSD/src/libexec/crond/do_command.c,v $
 * $Revision: 1.1.1.1 $
 * $Log: do_command.c,v $
 * Revision 1.1.1.1  1993/06/12  14:55:03  rgrimes
 * Initial import, 0.1 + pk 0.2.4-B1
 *
 * Revision 2.1  90/07/18  00:23:38  vixie
 * Baseline for 4.4BSD release
 * 
 * Revision 2.0  88/12/10  04:57:44  vixie
 * V2 Beta
 * 
 * Revision 1.5  88/11/29  13:06:06  vixie
 * seems to work on Ultrix 3.0 FT1
 * 
 * Revision 1.4  87/05/02  17:33:35  paul
 * baseline for mod.sources release
 * 
 * Revision 1.3  87/04/09  00:03:58  paul
 * improved data hiding, locality of declaration/references
 * fixed a rs@mirror bug by redesigning the mailto stuff completely
 * 
 * Revision 1.2  87/03/19  12:46:24  paul
 * implemented suggestions from rs@mirror (Rich $alz):
 *    MAILTO="" means no mail should be sent
 *    various fixes of bugs or lint complaints
 *    put a To: line in the mail message
 * 
 * Revision 1.1  87/01/26  23:47:00  paul
 * Initial revision
 *
 * PATCHES MAGIC                LEVEL   PATCH THAT GOT US HERE
 * --------------------         -----   ----------------------
 * CURRENT PATCH LEVEL:         1       00131
 * --------------------         -----   ----------------------
 *
 * 06 Apr 93	Adam Glass	Fixes so it compiles quitely
 *
 */

/* Copyright 1988,1990 by Paul Vixie
 * All rights reserved
 *
 * Distribute freely, except: don't remove my name from the source or
 * documentation (don't take credit for my work), mark your changes (don't
 * get me blamed for your possible bugs), don't alter or remove this
 * notice.  May be sold if buildable source is provided to buyer.  No
 * warrantee of any kind, express or implied, is included with this
 * software; use at your own risk, responsibility for damages (if any) to
 * anyone resulting from the use of this software rests entirely with the
 * user.
 *
 * Send bug reports, bug fixes, enhancements, requests, flames, etc., and
 * I'll try to keep a version up to date.  I can be reached as follows:
 * Paul Vixie, 329 Noe Street, San Francisco, CA, 94114, (415) 864-7013,
 * paul@vixie.sf.ca.us || {hoptoad,pacbell,decwrl,crash}!vixie!paul
 */


#include "cron.h"
#include <signal.h>
#include <pwd.h>
#if defined(BSD)
# include <sys/wait.h>
#endif /*BSD*/
#if defined(sequent)
# include <strings.h>
# include <sys/universe.h>
#endif


void
do_command(cmd, u)
	char	*cmd;
	user	*u;
{
	extern int	fork(), _exit();
	extern void	child_process(), log_it();
	extern char	*env_get();

	Debug(DPROC, ("[%d] do_command(%s, (%s,%d,%d))\n",
		getpid(), cmd, env_get(USERENV, u->envp), u->uid, u->gid))

	/* fork to become asynchronous -- parent process is done immediately,
	 * and continues to run the normal cron code, which means return to
	 * tick().  the child and grandchild don't leave this function, alive.
	 *
	 * vfork() is unsuitable, since we have much to do, and the parent
	 * needs to be able to run off and fork other processes.
	 */
	switch (fork())
	{
	case -1:
		log_it("CROND",getpid(),"error","can't fork");
		break;
	case 0:
		/* child process */
		child_process(cmd, u);
		Debug(DPROC, ("[%d] child process done, exiting\n", getpid()))
		_exit(OK_EXIT);
		break;
	}
	Debug(DPROC, ("[%d] main process returning to work\n", getpid()))
}


void
child_process(cmd, u)
	char	*cmd;
	user	*u;
{
	extern struct passwd	*getpwnam();
	extern void	sigpipe_func(), be_different(), log_it();
	extern int	VFORK();
	extern char	*index(), *env_get();

	auto int	stdin_pipe[2], stdout_pipe[2];
	register char	*input_data, *usernm, *mailto;
	auto int	children = 0;
#if defined(sequent)
	extern void	do_univ();
#endif

	Debug(DPROC, ("[%d] child_process('%s')\n", getpid(), cmd))

	/* mark ourselves as different to PS command watchers by upshifting
	 * our program name.  This has no effect on some kernels.
	 */
	{
		register char	*pch;

		for (pch = ProgramName;  *pch;  pch++)
			*pch = MkUpper(*pch);
	}

	/* discover some useful and important environment settings
	 */
	usernm = env_get(USERENV, u->envp);
	mailto = env_get("MAILTO", u->envp);

#if defined(BSD)
	/* our parent is watching for our death by catching SIGCHLD.  we
	 * do not care to watch for our children's deaths this way -- we
	 * use wait() explictly.  so we have to disable the signal (which
	 * was inherited from the parent).
	 *
	 * this isn't needed for system V, since our parent is already
	 * SIG_IGN on SIGCLD -- which, hopefully, will cause children to
	 * simply vanish when they die.
	 */
	(void) signal(SIGCHLD, SIG_IGN);
#endif /*BSD*/

	/* create some pipes to talk to our future child
	 */
	pipe(stdin_pipe);	/* child's stdin */
	pipe(stdout_pipe);	/* child's stdout */
	
	/* since we are a forked process, we can diddle the command string
	 * we were passed -- nobody else is going to use it again, right?
	 *
	 * if a % is present in the command, previous characters are the
	 * command, and subsequent characters are the additional input to
	 * the command.  Subsequent %'s will be transformed into newlines,
	 * but that happens later.
	 */
	if (NULL == (input_data = index(cmd, '%')))
	{
		/* no %.  point input_data at a null string.
		 */
		input_data = "";
	}
	else
	{
		/* % found.  replace with a null (remember, we're a forked
		 * process and the string won't be reused), and increment
		 * input_data to point at the following character.
		 */
		*input_data++ = '\0';
	}

	/* fork again, this time so we can exec the user's command.  Vfork()
	 * is okay this time, since we are going to exec() pretty quickly.
	 * I'm assuming that closing pipe ends &whatnot will not affect our
	 * suspended pseudo-parent/alter-ego.
	 */
	if (VFORK() == 0)
	{
		Debug(DPROC, ("[%d] grandchild process VFORK()'ed\n", getpid()))

		/* write a log message.  we've waited this long to do it
		 * because it was not until now that we knew the PID that
		 * the actual user command shell was going to get and the
		 * PID is part of the log message.
		 */
#ifdef LOG_FILE
		{
			extern char *mkprints();
			char *x = mkprints(cmd, strlen(cmd));

			log_it(usernm, getpid(), "CMD", x);
			free(x);
		}
#endif

		/* get new pgrp, void tty, etc.
		 */
		be_different();

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

		/* close the pipes we just dup'ed.  The resources will remain,
		 * since they've been dup'ed... :-)...
		 */
		close(stdin_pipe[READ_PIPE]);
		close(stdout_pipe[WRITE_PIPE]);

# if defined(sequent)
		/* set our login universe.  Do this in the grandchild
		 * so that the child can invoke /usr/lib/sendmail
		 * without surprises.
		 */
		do_univ(u);
# endif

		/* set our directory, uid and gid.  Set gid first, since once
		 * we set uid, we've lost root privledges.  (oops!)
		 */
		setgid(u->gid);
# if defined(BSD)
		initgroups(env_get(USERENV, u->envp), u->gid);
# endif
		setuid(u->uid);		/* you aren't root after this... */
		chdir(env_get("HOME", u->envp));

		/* exec the command.
		 */
		{
			char	*shell = env_get("SHELL", u->envp);

# if DEBUGGING
			if (DebugFlags & DTEST) {
				fprintf(stderr,
				"debug DTEST is on, not exec'ing command.\n");
				fprintf(stderr,
				"\tcmd='%s' shell='%s'\n", cmd, shell);
				_exit(OK_EXIT);
			}
# endif /*DEBUGGING*/
			/* normally you can't put debugging stuff here because
			 * it gets mailed with the command output.
			 */
			/*
			Debug(DPROC, ("[%d] execle('%s', '%s', -c, '%s')\n",
					getpid(), shell, shell, cmd))
			 */

# ifdef bad_idea
			/* files writable by non-owner are a no-no
			 */
			{
				struct stat sb;

				if (0 != stat(cmd, &sb)) {
					fputs("crond: stat(2): ", stderr);
					perror(cmd);
					_exit(ERROR_EXIT);
				} else if (sb.st_mode & 022) {
					fprintf(stderr,
					"crond: %s writable by nonowner\n",
						cmd);
					_exit(ERROR_EXIT);
				} else if (sb.st_uid & 022) {
					fprintf(stderr,
					"crond: %s owned by uid %d\n",
						cmd, sb.st_uid);
					_exit(ERROR_EXIT);
				}
			}
# endif /*bad_idea*/

			execle(shell, shell, "-c", cmd, (char *)0, u->envp);
			fprintf(stderr, "execl: couldn't exec `%s'\n", shell);
			perror("execl");
			_exit(ERROR_EXIT);
		}
	}

	children++;

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
	 * we would block here.  the solution, of course, is to fork again.
	 */

	if (*input_data && fork() == 0) {
		register FILE	*out = fdopen(stdin_pipe[WRITE_PIPE], "w");
		register int	need_newline = FALSE;
		register int	escaped = FALSE;
		register int	ch;

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
		while (ch = *input_data++)
		{
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

	children++;

	/*
	 * read output from the grandchild.  it's stderr has been redirected to
	 * it's stdout, which has been redirected to our pipe.  if there is any
	 * output, we'll be mailing it to the user whose crontab this is...
	 * when the grandchild exits, we'll get EOF.
	 */

	Debug(DPROC, ("[%d] child reading output from grandchild\n", getpid()))

	{
		register FILE	*in = fdopen(stdout_pipe[READ_PIPE], "r");
		register int	ch = getc(in);

		if (ch != EOF)
		{
			register FILE	*mail;
			register int	bytes = 1;
			union wait	status;

			Debug(DPROC|DEXT,
				("[%d] got data (%x:%c) from grandchild\n",
					getpid(), ch, ch))

			/* get name of recipient.  this is MAILTO if set to a
			 * valid local username; USER otherwise.
			 */
			if (mailto)
			{
				/* MAILTO was present in the environment
				 */
				if (!*mailto)
				{
					/* ... but it's empty. set to NULL
					 */
					mailto = NULL;
				}
			}
			else
			{
				/* MAILTO not present, set to USER.
				 */
				mailto = usernm;
			}
		
			/* if we are supposed to be mailing, MAILTO will
			 * be non-NULL.  only in this case should we set
			 * up the mail command and subjects and stuff...
			 */

			if (mailto)
			{
				extern FILE	*popen();
				extern char	*print_cmd();
				register char	**env;
				auto char	mailcmd[MAX_COMMAND];
				auto char	hostname[MAXHOSTNAMELEN];

				(void) gethostname(hostname, MAXHOSTNAMELEN);
				(void) sprintf(mailcmd, MAILCMD, mailto);
				if (!(mail = popen(mailcmd, "w")))
				{
					perror(MAILCMD);
					(void) _exit(ERROR_EXIT);
				}
				fprintf(mail, "From: root (Cron Daemon)\n");
				fprintf(mail, "To: %s\n", mailto);
				fprintf(mail,
				"Subject: cron for %s@%s said this\n",
					usernm, first_word(hostname, ".")
				);
				fprintf(mail, "Date: %s", ctime(&TargetTime));
				fprintf(mail, "X-Cron-Cmd: <%s>\n", cmd);
				for (env = u->envp;  *env;  env++)
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

			while (EOF != (ch = getc(in)))
			{
				bytes++;
				if (mailto)
					putc(ch, mail);
			}

			/* only close pipe if we opened it -- i.e., we're
			 * mailing...
			 */

			if (mailto) {
				Debug(DPROC, ("[%d] closing pipe to mail\n",
					getpid()))
				/* Note: the pclose will probably see
				 * the termination of the grandchild
				 * in addition to the mail process, since
				 * it (the grandchild) is likely to exit
				 * after closing its stdout.
				 */
				status.w_status = pclose(mail);
			}

			/* if there was output and we could not mail it,
			 * log the facts so the poor user can figure out
			 * what's going on.
			 */
			if (mailto && status.w_status) {
				char buf[MAX_TEMPSTR];

				sprintf(buf,
			"mailed %d byte%s of output but got status 0x%04x\n",
					bytes, (bytes==1)?"":"s",
					status.w_status);
				log_it(usernm, getpid(), "MAIL", buf);
			}

		} /*if data from grandchild*/

		Debug(DPROC, ("[%d] got EOF from grandchild\n", getpid()))

		fclose(in);	/* also closes stdout_pipe[READ_PIPE] */
	}

#if defined(BSD)
	/* wait for children to die.
	 */
	for (;  children > 0;  children--)
	{
		int		pid;
		union wait	waiter;

		Debug(DPROC, ("[%d] waiting for grandchild #%d to finish\n",
			getpid(), children))
		pid = wait((int *) &waiter);
		if (pid < OK) {
			Debug(DPROC, ("[%d] no more grandchildren--mail written?\n",
				getpid()))
			break;
		}
		Debug(DPROC, ("[%d] grandchild #%d finished, status=%04x",
			getpid(), pid, waiter.w_status))
		if (waiter.w_coredump)
			Debug(DPROC, (", dumped core"))
		Debug(DPROC, ("\n"))
	}
#endif /*BSD*/
}


#if defined(sequent)
/* Dynix (Sequent) hack to put the user associated with
 * the passed user structure into the ATT universe if
 * necessary.  We have to dig the gecos info out of
 * the user's password entry to see if the magic
 * "universe(att)" string is present.  If we do change
 * the universe, also set "LOGNAME".
 */

void
do_univ(u)
	user	*u;
{
	struct	passwd	*p;
	char	*s;
	int	i;
	char	envstr[MAX_ENVSTR], **env_set();

	p = getpwuid(u->uid);
	(void) endpwent();

	if (p == NULL)
		return;

	s = p->pw_gecos;

	for (i = 0; i < 4; i++)
	{
		if ((s = index(s, ',')) == NULL)
			return;
		s++;
	}
	if (strcmp(s, "universe(att)"))
		return;

	(void) sprintf(envstr, "LOGNAME=%s", p->pw_name);
	u->envp = env_set(u->envp, envstr);

	(void) universe(U_ATT);
}
#endif
