
/*
 * This is an rcmd() replacement originally by 
 * Chris Siebenmann <cks@utcc.utoronto.ca>.
 */

#ifndef lint
static const char rcsid[] =
	"$Id$";
#endif /* not lint */

#include	"defs.h"

#if	!defined(DIRECT_RCMD)

#include      <sys/socket.h>
#include      <sys/wait.h>
#include      <signal.h>
#include      <netdb.h>

static char *
xbasename(s)
	char *s;
{
	char *ret;

	ret = strrchr(s, '/');
	if (ret && ret[1])
		return (ret + 1);
	return s;
}


/*
 * This is a replacement rcmd() function that uses the rsh(1c)
 * program in place of a direct rcmd() function call so as to
 * avoid having to be root.
 */
int
rshrcmd(ahost, port, luser, ruser, cmd, fd2p)
	char  	**ahost;
	u_short	port;
	char	*luser, *ruser, *cmd;
	int	*fd2p;
{
	int             cpid;
	struct hostent  *hp;
	int             sp[2];

	/* insure that we are indeed being used as we thought. */
	if (fd2p != 0)
		return -1;
	/* validate remote hostname. */
	hp = gethostbyname(*ahost);
	if (hp == 0) {
		error("%s: unknown host", *ahost);
		return -1;
	}
	/* *ahost = hp->h_name; *//* This makes me nervous. */

	/* get a socketpair we'll use for stdin and stdout. */
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, sp) < 0) {
		error("socketpair(AF_UNIX, SOCK_STREAM, 0) failed: %s.", 
		      strerror(errno));
		return -1;
	}

	cpid = fork();
	if (cpid < 0) {
		error("fork failed: %s.", strerror(errno));
		return -1;      /* error. */
	}
	if (cpid == 0) {
		/* child. we use sp[1] to be stdin/stdout, and close
		   sp[0]. */
		(void) close(sp[0]);
		if (dup2(sp[1], 0) < 0 || dup2(0,1) < 0) {
			error("dup2 failed: %s.", strerror(errno));
			_exit(255);
		}
		/* fork again to lose parent. */
		cpid = fork();
		if (cpid < 0) {
			error("fork to lose parent failed: %s.", strerror(errno));
			_exit(255);
		}
		if (cpid > 0)
			_exit(0);
		/* in grandchild here. */

		/*
		 * If we are rdist'ing to "localhost" as the same user
		 * as we are, then avoid running remote shell for efficiency.
		 */
		if (strcmp(*ahost, "localhost") == 0 &&
		    strcmp(luser, ruser) == 0) {
			execlp(_PATH_BSHELL, xbasename(_PATH_BSHELL), "-c",
			       cmd, (char *) NULL);
			error("execlp %s failed: %s.", _PATH_BSHELL, strerror(errno));
		} else {
			execlp(path_rsh, xbasename(path_rsh), 
			       *ahost, "-l", ruser, cmd, (char *) NULL);
			error("execlp %s failed: %s.", path_rsh,
				strerror(errno));
		}
		_exit(255);
	}
	if (cpid > 0) {
		/* parent. close sp[1], return sp[0]. */
		(void) close(sp[1]);
		/* reap child. */
		(void) wait(0);
		return sp[0];
	}
	/*NOTREACHED*/
	return (-1);
}

#endif	/* !DIRECT_RCMD */
