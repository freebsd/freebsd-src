/*	$OpenBSD: rcmdsh.c,v 1.5 1998/04/25 16:23:58 millert Exp $	*/ 

/*
 * This is an rcmd() replacement originally by 
 * Chris Siebenmann <cks@utcc.utoronto.ca>.
 *
 * $FreeBSD$
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char *rcsid = "$FreeBSD$"
#endif /* LIBC_SCCS and not lint */

#include      <sys/types.h>
#include      <sys/socket.h>
#include      <sys/wait.h>
#include      <signal.h>
#include      <errno.h>
#include      <netdb.h>
#include      <stdio.h>
#include      <string.h>
#include      <pwd.h>
#include      <paths.h>
#include      <unistd.h>

#ifndef _PATH_RSH
#define _PATH_RSH "/usr/bin/rsh"
#endif

/*
 * This is a replacement rcmd() function that uses the rsh(1)
 * program in place of a direct rcmd(3) function call so as to
 * avoid having to be root.  Note that rport is ignored.
 */
/* ARGSUSED */
int
rcmdsh(ahost, rport, locuser, remuser, cmd, rshprog)
	char **ahost;
	int rport;
	const char *locuser, *remuser, *cmd;
	char *rshprog;
{
	struct hostent *hp;
	int cpid, sp[2];
	char *p;
	struct passwd *pw;

	/* What rsh/shell to use. */
	if (rshprog == NULL)
		rshprog = _PATH_RSH;

	/* locuser must exist on this host. */
	if ((pw = getpwnam(locuser)) == NULL) {
		(void) fprintf(stderr, "rcmdsh: unknown user: %s\n", locuser);
		return(-1);
	}

	/* Validate remote hostname. */
	if (strcmp(*ahost, "localhost") != 0) {
		if ((hp = gethostbyname(*ahost)) == NULL) {
			herror(*ahost);
			return(-1);
		}
		*ahost = hp->h_name;
	}

	/* Get a socketpair we'll use for stdin and stdout. */
	if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC, sp) < 0) {
		perror("rcmdsh: socketpair");
		return(-1);
	}

	cpid = fork();
	if (cpid < 0) {
		perror("rcmdsh: fork failed");
		return(-1);
	} else if (cpid == 0) {
		/*
		 * Child.  We use sp[1] to be stdin/stdout, and close sp[0].
		 */
		(void) close(sp[0]);
		if (dup2(sp[1], 0) < 0 || dup2(0, 1) < 0) {
			perror("rcmdsh: dup2 failed");
			_exit(255);
		}
		/* Fork again to lose parent. */
		cpid = fork();
		if (cpid < 0) {
			perror("rcmdsh: fork to lose parent failed");
			_exit(255);
		}
		if (cpid > 0)
			_exit(0);

		/* In grandchild here.  Become local user for rshprog. */
		if (setuid(pw->pw_uid)) {
			(void) fprintf(stderr, "rcmdsh: setuid(%u): %s\n",
				       pw->pw_uid, strerror(errno));
			_exit(255);
		}

		/*
		 * If remote host is "localhost" and local and remote user
		 * are the same, avoid running remote shell for efficiency.
		 */
		if (!strcmp(*ahost, "localhost") && !strcmp(locuser, remuser)) {
			if (pw->pw_shell[0] == '\0')
				rshprog = _PATH_BSHELL;
			else
				rshprog = pw->pw_shell;
			p = strrchr(rshprog, '/');
			execlp(rshprog, p ? p+1 : rshprog, "-c", cmd,
			       (char *) NULL);
		} else {
			p = strrchr(rshprog, '/');
			execlp(rshprog, p ? p+1 : rshprog, *ahost, "-l",
			       remuser, cmd, (char *) NULL);
		}
		(void) fprintf(stderr, "rcmdsh: execlp %s failed: %s\n",
			       rshprog, strerror(errno));
		_exit(255);
	} else {
		/* Parent. close sp[1], return sp[0]. */
		(void) close(sp[1]);
		/* Reap child. */
		(void) wait(NULL);
		return(sp[0]);
	}
	/* NOTREACHED */
}
