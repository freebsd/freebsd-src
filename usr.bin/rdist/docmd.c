/*
 * Copyright (c) 1983, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
#if 0
static char sccsid[] = "From: @(#)docmd.c	8.1 (Berkeley) 6/9/93";
#endif
static const char rcsid[] =
  "$FreeBSD: src/usr.bin/rdist/docmd.c,v 1.12 1999/08/28 01:05:06 peter Exp $";
#endif /* not lint */

#include "defs.h"
#include <setjmp.h>
#include <netdb.h>
#include <regex.h>

FILE	*lfp;			/* log file for recording files updated */
struct	subcmd *subcmds;	/* list of sub-commands for current cmd */
jmp_buf	env;

static int	 makeconn __P((char *));
static int	 okname __P((char *));
static void	 closeconn __P((void));
static void	 cmptime __P((char *));
static void	 doarrow __P((char **,
		    struct namelist *, char *, struct subcmd *));
static void	 dodcolon __P((char **,
		    struct namelist *, char *, struct subcmd *));
static void	 notify __P((char *, char *, struct namelist *, time_t));
static void	 rcmptime __P((struct stat *));
static int	 remotecmd __P((char *, char *, char *, char *));

/*
 * Do the commands in cmds (initialized by yyparse).
 */
void
docmds(dhosts, argc, argv)
	char **dhosts;
	int argc;
	char **argv;
{
	register struct cmd *c;
	register struct namelist *f;
	register char **cpp;
	extern struct cmd *cmds;

	signal(SIGHUP, cleanup);
	signal(SIGINT, cleanup);
	signal(SIGQUIT, cleanup);
	signal(SIGTERM, cleanup);

	for (c = cmds; c != NULL; c = c->c_next) {
		if (dhosts != NULL && *dhosts != NULL) {
			for (cpp = dhosts; *cpp; cpp++)
				if (strcmp(c->c_name, *cpp) == 0)
					goto fndhost;
			continue;
		}
	fndhost:
		if (argc) {
			for (cpp = argv; *cpp; cpp++) {
				if (c->c_label != NULL &&
				    strcmp(c->c_label, *cpp) == 0) {
					cpp = NULL;
					goto found;
				}
				for (f = c->c_files; f != NULL; f = f->n_next)
					if (strcmp(f->n_name, *cpp) == 0)
						goto found;
			}
			continue;
		} else
			cpp = NULL;
	found:
		switch (c->c_type) {
		case ARROW:
			doarrow(cpp, c->c_files, c->c_name, c->c_cmds);
			break;
		case DCOLON:
			dodcolon(cpp, c->c_files, c->c_name, c->c_cmds);
			break;
		default:
			fatal("illegal command type %d\n", c->c_type);
		}
	}
	closeconn();
}

/*
 * Process commands for sending files to other machines.
 */
static void
doarrow(filev, files, rhost, cmds)
	char **filev;
	struct namelist *files;
	char *rhost;
	struct subcmd *cmds;
{
	register struct namelist *f;
	register struct subcmd *sc;
	register char **cpp;
	int n, ddir, opts = options;

	if (debug)
		printf("doarrow(%p, %s, %p)\n", files, rhost, cmds);

	if (files == NULL) {
		error("no files to be updated\n");
		return;
	}

	subcmds = cmds;
	ddir = files->n_next != NULL;	/* destination is a directory */
	if (nflag)
		printf("updating host %s\n", rhost);
	else {
		if (setjmp(env))
			goto done;
		signal(SIGPIPE, lostconn);
		if (!makeconn(rhost))
			return;
		if ((lfp = fopen(tempfile, "w")) == NULL) {
			fatal("cannot open %s\n", tempfile);
			exit(1);
		}
	}
	for (f = files; f != NULL; f = f->n_next) {
		if (filev) {
			for (cpp = filev; *cpp; cpp++)
				if (strcmp(f->n_name, *cpp) == 0)
					goto found;
			if (!nflag)
				(void) fclose(lfp);
			continue;
		}
	found:
		n = 0;
		for (sc = cmds; sc != NULL; sc = sc->sc_next) {
			if (sc->sc_type != INSTALL)
				continue;
			n++;
			install(f->n_name, sc->sc_name,
				sc->sc_name == NULL ? 0 : ddir, sc->sc_options);
			opts = sc->sc_options;
		}
		if (n == 0)
			install(f->n_name, NULL, 0, options);
	}
done:
	if (!nflag) {
		(void) signal(SIGPIPE, cleanup);
		if (lfp)
			(void) fclose(lfp);
		lfp = NULL;
	}
	for (sc = cmds; sc != NULL; sc = sc->sc_next)
		if (sc->sc_type == NOTIFY)
			notify(tempfile, rhost, sc->sc_args, 0);
	if (!nflag) {
		struct linkbuf *nextihead;

		(void) unlink(tempfile);
		for (; ihead != NULL; ihead = nextihead) {
			nextihead = ihead->nextp;
			if ((opts & IGNLNKS) || ihead->count == 0)
				continue;
			log(lfp, "%s: Warning: missing links\n",
				ihead->pathname);
			free(ihead);
		}
	}
}

static int remotecmd(rhost, luser, ruser, cmd)
	char *rhost;
	char *luser, *ruser;
	char *cmd;
{
	int desc;
	static int port = -1;

	if (debug) {
		printf("local user = %s remote user = %s\n", luser, ruser);
		printf("Remote command = '%s'\n", cmd);
	}

	(void) fflush(stdout);
	(void) fflush(stderr);
	(void) signal(SIGALRM, cleanup);
	(void) alarm(RTIMEOUT);

	if (geteuid() == 0 && strcmp(path_rsh, _PATH_RSH) == 0) {
		if (debug) 
			printf("I am root, therefore direct rcmd\n");

		(void) signal(SIGPIPE, cleanup);

		if (port < 0) {
			struct servent *sp;
		
			if ((sp = getservbyname("shell", "tcp")) == NULL)
				fatal("shell/tcp: unknown service");
			port = sp->s_port;
		}

		desc = rcmd(&rhost, port, luser, ruser, cmd, 0);
	} else {
		if (debug)
			printf("Remote shell command = '%s'\n", path_rsh);
		(void) signal(SIGPIPE, SIG_IGN);
		desc = rshrcmd(&rhost, -1, luser, ruser, cmd, 0);
		if (desc > 0)
			(void) signal(SIGPIPE, cleanup);
	}

	(void) alarm(0);

	return(desc);
}


/*
 * Create a connection to the rdist server on the machine rhost.
 */
static int
makeconn(rhost)
	char *rhost;
{
	register char *ruser, *cp;
	static char *cur_host = NULL;
	static int port = -1;
	char tuser[20];
	int n;
	extern char user[];
#if defined(DIRECT_RCMD)
	extern int userid;
#endif

	if (debug)
		printf("makeconn(%s)\n", rhost);

	if (cur_host != NULL && rem >= 0) {
		if (strcmp(cur_host, rhost) == 0)
			return(1);
		closeconn();
	}
	cur_host = rhost;
	cp = index(rhost, '@');
	if (cp != NULL) {
		char c = *cp;

		*cp = '\0';
		strncpy(tuser, rhost, sizeof(tuser)-1);
		*cp = c;
		rhost = cp + 1;
		ruser = tuser;
		if (*ruser == '\0')
			ruser = user;
		else if (!okname(ruser))
			return(0);
	} else
		ruser = user;
	if (!qflag)
		printf("updating host %s\n", rhost);
	(void) snprintf(buf, sizeof(buf), "%s -Server%s",
	    _PATH_RDIST, qflag ? " -q" : "");
	if (port < 0) {
		struct servent *sp;

		if ((sp = getservbyname("shell", "tcp")) == NULL)
			fatal("shell/tcp: unknown service");
		port = sp->s_port;
	}

	if (debug) {
		printf("port = %d, luser = %s, ruser = %s\n", ntohs(port), user, ruser);
		printf("buf = %s\n", buf);
	}

	fflush(stdout);
#if defined(DIRECT_RCMD)
	seteuid(0);
	rem = rcmd(&rhost, port, user, ruser, buf, 0);
	seteuid(userid);
#else
	rem = remotecmd(rhost, user, ruser, buf);
#endif
	if (rem < 0)
		return(0);
	cp = buf;
	if (read(rem, cp, 1) != 1)
		lostconn(0);
	if (*cp == 'V') {
		do {
			if (read(rem, cp, 1) != 1)
				lostconn(0);
		} while (*cp++ != '\n' && cp < &buf[BUFSIZ]);
		*--cp = '\0';
		cp = buf;
		n = 0;
		while (*cp >= '0' && *cp <= '9')
			n = (n * 10) + (*cp++ - '0');
		if (*cp == '\0' && n == VERSION)
			return(1);
		error("connection failed: version numbers don't match (local %d, remote %d)\n", VERSION, n);
	} else {
		error("connection failed: version numbers don't match\n");
		error("got unexpected input:");
		do {
			error("%c", *cp);
		} while (*cp != '\n' && read(rem, cp, 1) == 1);
	}
	closeconn();
	return(0);
}

/*
 * Signal end of previous connection.
 */
static void
closeconn()
{
	if (debug)
		printf("closeconn()\n");

	if (rem >= 0) {
		(void) write(rem, "\2\n", 2);
		(void) close(rem);
		rem = -1;
	}
}

void
lostconn(signo)
	int signo;
{
	if (iamremote)
		cleanup(0);
	log(lfp, "rdist: lost connection\n");
	longjmp(env, 1);
}

static int
okname(name)
	register char *name;
{
	register char *cp = name;
	register int c;

	do {
		c = *cp;
		if (c & 0200)
			goto bad;
		if (!isalpha(c) && !isdigit(c) && c != '_' && c != '-')
			goto bad;
		cp++;
	} while (*cp);
	return(1);
bad:
	error("invalid user name %s\n", name);
	return(0);
}

time_t	lastmod;
FILE	*tfp;
extern	char target[], *tp;

/*
 * Process commands for comparing files to time stamp files.
 */
static void
dodcolon(filev, files, stamp, cmds)
	char **filev;
	struct namelist *files;
	char *stamp;
	struct subcmd *cmds;
{
	register struct subcmd *sc;
	register struct namelist *f;
	register char **cpp;
	struct timeval tv[2];
	struct timezone tz;
	struct stat stb;

	if (debug)
		printf("dodcolon()\n");

	if (files == NULL) {
		error("no files to be updated\n");
		return;
	}
	if (stat(stamp, &stb) < 0) {
		error("%s: %s\n", stamp, strerror(errno));
		return;
	}
	if (debug)
		printf("%s: %ld\n", stamp, stb.st_mtime);

	subcmds = cmds;
	lastmod = stb.st_mtime;
	if (nflag || (options & VERIFY))
		tfp = NULL;
	else {
		if ((tfp = fopen(tempfile, "w")) == NULL) {
			error("%s: %s\n", stamp, strerror(errno));
			return;
		}
		(void) gettimeofday(&tv[0], &tz);
		tv[1] = tv[0];
		(void) utimes(stamp, tv);
	}

	for (f = files; f != NULL; f = f->n_next) {
		if (filev) {
			for (cpp = filev; *cpp; cpp++)
				if (strcmp(f->n_name, *cpp) == 0)
					goto found;
			continue;
		}
	found:
		tp = NULL;
		cmptime(f->n_name);
	}

	if (tfp != NULL)
		(void) fclose(tfp);
	for (sc = cmds; sc != NULL; sc = sc->sc_next)
		if (sc->sc_type == NOTIFY)
			notify(tempfile, NULL, sc->sc_args, lastmod);
	if (!nflag && !(options & VERIFY))
		(void) unlink(tempfile);
}

/*
 * Compare the mtime of file to the list of time stamps.
 */
static void
cmptime(name)
	char *name;
{
	struct stat stb;

	if (debug)
		printf("cmptime(%s)\n", name);

	if (except(name))
		return;

	if (nflag) {
		printf("comparing dates: %s\n", name);
		return;
	}

	/*
	 * first time cmptime() is called?
	 */
	if (tp == NULL) {
		if (exptilde(target, name, sizeof(target)) == NULL)
			return;
		tp = name = target;
		while (*tp)
			tp++;
	}
	if (access(name, 4) < 0 || stat(name, &stb) < 0) {
		error("%s: %s\n", name, strerror(errno));
		return;
	}

	switch (stb.st_mode & S_IFMT) {
	case S_IFREG:
		break;

	case S_IFDIR:
		rcmptime(&stb);
		return;

	default:
		error("%s: not a plain file\n", name);
		return;
	}

	if (stb.st_mtime > lastmod)
		log(tfp, "new: %s\n", name);
}

static void
rcmptime(st)
	struct stat *st;
{
	register DIR *d;
	register struct dirent *dp;
	register char *cp;
	char *otp;
	int len;

	if (debug)
		printf("rcmptime(%p)\n", st);

	if ((d = opendir(target)) == NULL) {
		error("%s: %s\n", target, strerror(errno));
		return;
	}
	otp = tp;
	len = tp - target;
	while ((dp = readdir(d))) {
		if (!strcmp(dp->d_name, ".") || !strcmp(dp->d_name, ".."))
			continue;
		if (len + 1 + strlen(dp->d_name) >= BUFSIZ - 1) {
			error("%s/%s: Name too long\n", target, dp->d_name);
			continue;
		}
		tp = otp;
		*tp++ = '/';
		cp = dp->d_name;
		while ((*tp++ = *cp++))
			;
		tp--;
		cmptime(target);
	}
	closedir(d);
	tp = otp;
	*tp = '\0';
}

/*
 * Notify the list of people the changes that were made.
 * rhost == NULL if we are mailing a list of changes compared to at time
 * stamp file.
 */
static void
notify(file, rhost, to, lmod)
	char *file, *rhost;
	register struct namelist *to;
	time_t lmod;
{
	register int fd, len;
	struct stat stb;
	FILE *pf;

	if ((options & VERIFY) || to == NULL)
		return;
	if (!qflag) {
		printf("notify ");
		if (rhost)
			printf("@%s ", rhost);
		prnames(to);
	}
	if (nflag)
		return;

	if ((fd = open(file, 0)) < 0) {
		error("%s: %s\n", file, strerror(errno));
		return;
	}
	if (fstat(fd, &stb) < 0) {
		error("%s: %s\n", file, strerror(errno));
		(void) close(fd);
		return;
	}
	if (stb.st_size == 0) {
		(void) close(fd);
		return;
	}
	/*
	 * Create a pipe to mailling program.
	 */
	(void) snprintf(buf, sizeof(buf), "%s -oi -t", _PATH_SENDMAIL);
	pf = popen(buf, "w");
	if (pf == NULL) {
		error("notify: \"%s\" failed\n", _PATH_SENDMAIL);
		(void) close(fd);
		return;
	}
	/*
	 * Output the proper header information.
	 */
	fprintf(pf, "From: rdist (Remote distribution program)\n");
	fprintf(pf, "To:");
	if (!any('@', to->n_name) && rhost != NULL)
		fprintf(pf, " %s@%s", to->n_name, rhost);
	else
		fprintf(pf, " %s", to->n_name);
	to = to->n_next;
	while (to != NULL) {
		if (!any('@', to->n_name) && rhost != NULL)
			fprintf(pf, ", %s@%s", to->n_name, rhost);
		else
			fprintf(pf, ", %s", to->n_name);
		to = to->n_next;
	}
	putc('\n', pf);
	if (rhost != NULL)
		fprintf(pf, "Subject: files updated by rdist from %s to %s\n",
			host, rhost);
	else
		fprintf(pf, "Subject: files updated after %s\n", ctime(&lmod));
	putc('\n', pf);

	while ((len = read(fd, buf, BUFSIZ)) > 0)
		(void) fwrite(buf, 1, len, pf);
	(void) close(fd);
	(void) pclose(pf);
}

/*
 * Return true if name is in the list.
 */
int
inlist(list, file)
	struct namelist *list;
	char *file;
{
	register struct namelist *nl;

	for (nl = list; nl != NULL; nl = nl->n_next)
		if (!strcmp(file, nl->n_name))
			return(1);
	return(0);
}

/*
 * Return TRUE if file is in the exception list.
 */
int
except(file)
	char *file;
{
	register struct	subcmd *sc;
	register struct	namelist *nl;
	regex_t rx;
	int val;

	if (debug)
		printf("except(%s)\n", file);

	for (sc = subcmds; sc != NULL; sc = sc->sc_next) {
		if (sc->sc_type != EXCEPT && sc->sc_type != PATTERN)
			continue;
		for (nl = sc->sc_args; nl != NULL; nl = nl->n_next) {
			if (sc->sc_type == EXCEPT) {
				if (!strcmp(file, nl->n_name))
					return(1);
				continue;
			}
			val = regcomp(&rx, nl->n_name,
				      REG_EXTENDED | REG_NOSUB);
			if (!regexec(&rx, file, 0, 0, 0)) {
				regfree(&rx);
				return 1;
			}
			regfree(&rx);
		}
	}
	return(0);
}

char *
colon(cp)
	register char *cp;
{

	while (*cp) {
		if (*cp == ':')
			return(cp);
		if (*cp == '/')
			return(0);
		cp++;
	}
	return(0);
}
