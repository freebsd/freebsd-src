/*-
 * Copyright (c) 1996 by
 * Sean Eric Fagan <sef@kithrup.com>
 * David Nugent <davidn@blaze.net.au>
 * All rights reserved.
 *
 * Portions copyright (c) 1995,1997 by
 * Berkeley Software Design, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, is permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. This work was done expressly for inclusion into FreeBSD.  Other use
 *    is permitted provided this notation is included.
 * 4. Absolutely no warranty of function or purpose is made by the authors.
 * 5. Modifications may be freely made to this file providing the above
 *    conditions are met.
 *
 * Low-level routines relating to the user capabilities database
 *
 *	$Id: login_auth.c,v 1.7 1997/05/10 18:55:37 davidn Exp $
 */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <ctype.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <login_cap.h>
#include <stdarg.h>
#include <paths.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <err.h>
#include <libutil.h>

#ifdef	LOGIN_CAP_AUTH
/*
 * Comment from BSDI's authenticate.c module:
 * NOTE: THIS MODULE IS TO BE DEPRECATED.  FUTURE VERSIONS OF BSD/OS WILL
 * HAVE AN UPDATED API, THOUGH THESE FUNCTIONS WILL CONTINUE TO BE AVAILABLE
 * FOR BACKWARDS COMPATABILITY
 */


#define AUTHMAXSPOOL	(8 * 1024) /* Max size of authentication data */
#define	AUTHCOMM_FD	3	   /* Handle used to read/write auth data */

struct rmfiles {
    struct rmfiles  *next;
    char	    file[1];
};

struct authopts {
    struct authopts *next;
    char	    opt[1];
};

static char *spoolbuf = NULL;
static int spoolidx = 0;
static struct rmfiles *rmfirst = NULL;
static struct authopts *optfirst = NULL;


/*
 * Setup a known environment for all authentication scripts.
 */

static char *auth_environ[] = {
    "PATH=" _PATH_DEFPATH,
    "SHELL=" _PATH_BSHELL,
    NULL,
};



/*
 * nextline()
 * Get the next line from the data buffer collected from
 * the authentication program. This function relies on the
 * fact that lines are nul terminated.
 */

static char *
nextline(int *idx)
{
    char    *ptr = NULL;

    if (spoolbuf != NULL && *idx < spoolidx) {
	ptr = spoolbuf + *idx;
	*idx += strlen(ptr) + 1;
    }
    return ptr;
}


/*
 * spooldata()
 * Read data returned on authentication backchannel and
 * stuff it into our spool buffer. We also replace \n with nul
 * to make parsing easier later.
 */

static int
spooldata(int fd)
{

    if (spoolbuf)
	free(spoolbuf);
    spoolidx = 0;

    if (spoolbuf == NULL && (spoolbuf = malloc(AUTHMAXSPOOL)) == NULL)
	syslog(LOG_ERR, "authbuffer malloc: %m");

    else while (spoolidx < sizeof(spoolbuf) - 1) {
	int	r = read(fd, spoolbuf + spoolidx, sizeof(spoolbuf)-spoolidx);
	char	*b;

	if (r <= 0) {
	    spoolbuf[spoolidx] = '\0';
	    return 0;
	}
	/*
	 * Convert newlines into NULs to allow
	 * easier scanning of the file.
	 */
	while ((b = memchr(spoolbuf + spoolidx, '\n', r)) != NULL)
	    *b = '\0';
	spoolidx += r;
    }
    return -1;
}


/*
 * auth_check()
 * Starts an auth_script() for the given <user>, with a class <class>,
 * style <style>, and service <service>.  <style> is necessary,
 * as are <user> and <class>, but <service> is optional -- it defaults
 * to "login".
 * Since auth_script() expects an execl'able program name, authenticate()
 * also concatenates <style> to _PATH_AUTHPROG.
 * Lastly, calls auth_scan(0) to see if there are any "reject" statements,
 * or lack of "auth" statements.
 * Returns -1 on error, 0 on rejection, and >0 on success.
 * (See AUTH_* for the return values.)
 *
 */

int
auth_check(const char *name, const char *clss, const char *style,
	   const char *service, int *status)
{
    int	    _status;

    if (status == NULL)
	status = &_status;
    *status = 0;

    if (style != NULL) {
	char	path[MAXPATHLEN];

	if (service == NULL)
	    service = LOGIN_DEFSERVICE;

	snprintf(path, sizeof(path), _PATH_AUTHPROG "%s", style);
	if (auth_script(path, style, "-s", service, name, clss, 0))
	    status = 0;
	else
	    *status = auth_scan(0);

	return *status & AUTH_ALLOW;
    }
    return -1;
}


int
auth_response(const char *name, const char *class, const char *style,
	      const char *service, int *status,
	      const char *challenge, const char *response)
{
    int	    _status;

    if (status == NULL)
	status = &_status;
    *status = 0;

    if (style != NULL) {
	int	datalen;
	char    *data;

	if (service == NULL)
	    service = LOGIN_DEFSERVICE;

	datalen = strlen(challenge) + strlen(response) + 2;

	if ((data = malloc(datalen)) == NULL) {
	    syslog(LOG_ERR, "auth_response: %m");
	    warnx("internal resource failure");
	} else {
	    char    path[MAXPATHLEN];

	    snprintf(data, datalen, "%s%c%s", challenge, 0, response);
	    snprintf(path, sizeof(path), _PATH_AUTHPROG "%s", style);
	    if (auth_script_data(data, datalen, path, style, "-s", service,
				 name, class, 0))
		*status = 0;
	    else
		*status = auth_scan(0);
	    free(data);
	    return (*status & AUTH_ALLOW);
	}
    }
    return -1;
}


int
auth_approve(login_cap_t *lc, const char *name, const char *service)
{
    int	    r = -1;
    char    path[MAXPATHLEN];

    if (lc == NULL) {
	if (strlen(name) > MAXPATHLEN) {
	    syslog(LOG_ERR, "%s: username too long", name);
	    warnx("username too long");
	} else {
	    struct passwd   *pwd;
	    char	    *p;

	    pwd = getpwnam(name);
	    if (pwd == NULL && (p = strchr(name, '.')) != NULL) {
		int	i = p - name;

		if (i >= MAXPATHLEN)
		    i = MAXPATHLEN - 1;
		strncpy(path, name, i);
		path[i] = '\0';
		pwd = getpwnam(path); /* Fixed bug in BSDI code... */
	    }
	    if ((lc = login_getpwclass(pwd ? pwd->pw_class : NULL)) == NULL)
		warnx("unable to classify user '%s'", name);
	}
    }

    if (lc != NULL) {
	char	*approve;
	char	*s;

	if (service != NULL)
		service = LOGIN_DEFSERVICE;

	snprintf(path, sizeof(path), "approve-%s", service);

        if ((approve = login_getcapstr(lc, s = path, NULL, NULL)) == NULL &&
	    (approve = login_getcapstr(lc, s = "approve", NULL, NULL)) == NULL)
	    r = AUTH_OKAY;
	else {

	    if (approve[0] != '/') {
		syslog(LOG_ERR, "Invalid %s script: %s", s, approve);
		warnx("invalid path to approval script");
	    } else {
		char	*s;

		s = strrchr(approve, '/') + 1;
		if (auth_script(approve, s, name,
				lc->lc_class, service, 0) == 0 &&
		    (r = auth_scan(AUTH_OKAY) & AUTH_ALLOW) != 0)
		    auth_env();
	    }
	}
    }
    return r;
}


void
auth_env(void)
{
    int	    idx = 0;
    char    *line;

    while ((line = nextline(&idx)) != NULL) {
	if (!strncasecmp(line, BI_SETENV, sizeof(BI_SETENV)-1)) {
	    line += sizeof(BI_SETENV) - 1;
	    if (*line && isspace(*line)) {
		char	*name;
		char	ch, *p;

		while (*line && isspace(*line))
		    ++line;
		name = line;
		while (*line && !isspace(*line))
		    ++line;
		ch = *(p = line);
		if (*line)
		    ++line;
		if (setenv(name, line, 1))
		    warn("setenv(%s, %s)", name, line);
		*p = ch;
	    }
	}
    }
}


char *
auth_value(const char *what)
{
    int	    idx = 0;
    char    *line;

    while ((line = nextline(&idx)) != NULL) {
	if (!strncasecmp(line, BI_VALUE, sizeof(BI_VALUE)-1)) {
	    char    *name;

	    line += sizeof(BI_VALUE) - 1;
	    while (*line && isspace(*line))
		++line;
	    name = line;
	    if (*line) {
		int	i;
		char	ch, *p;

		ch = *(p = line);
		*line++ = '\0';
		i = strcmp(name, what);
		*p = ch;
		if (i == 0)
		    return auth_mkvalue(line);
	    }
	}
    }
    return NULL;
}

char *
auth_mkvalue(const char *value)
{
    char *big, *p;

    big = malloc(strlen(value) * 4 + 1);
    if (big != NULL) {
	for (p = big; *value; ++value) {
	    switch (*value) {
	    case '\r':
		*p++ = '\\';
		*p++ = 'r';
		break;
	    case '\n':
		*p++ = '\\';
		*p++ = 'n';
		break;
	    case '\\':
		*p++ = '\\';
		*p++ = *value;
		break;
	    case '\t':
	    case ' ':
		if (p == big)
		    *p++ = '\\';
		*p++ = *value;
		break;
	    default:
		if (!isprint(*value)) {
		    *p++ = '\\';
		    *p++ = ((*value >> 6) & 0x3) + '0';
		    *p++ = ((*value >> 3) & 0x7) + '0';
		    *p++ = ((*value     ) & 0x7) + '0';
		} else
		    *p++ = *value;
		break;
	    }
	}
	*p = '\0';
	big = realloc(big, strlen(big) + 1);
    }
    return big;
}


#define NARGC	63
static int
_auth_script(const char *data, int nbytes, const char *path, va_list ap)
{
    int		    r, argc, status;
    int		    pfd[2];
    pid_t	    pid;
    struct authopts *e;
    char	    *argv[NARGC+1];

    r = -1;
    argc = 0;
    for (e = optfirst; argc < (NARGC - 1) && e != NULL; e = e->next) {
	argv[argc++] = "-v";
	argv[argc++] = e->opt;
    }
    while (argc < NARGC && (argv[argc] = va_arg(ap, char *)) != NULL)
	++argc;
    argv[argc] = NULL;

    if (argc >= NARGC && va_arg(ap, char *))
	syslog(LOG_ERR, "too many arguments");
    else if (_secure_path(path, 0, 0) < 0) {
	syslog(LOG_ERR, "%s: path not secure", path);
	warnx("invalid script: %s", path);
    } else if (socketpair(PF_LOCAL, SOCK_STREAM, 0, pfd) < 0) {
	syslog(LOG_ERR, "unable to create backchannel %m");
	warnx("internal resource failure");
    } else switch (pid = fork()) {
    case -1:			/* fork() failure */
	close(pfd[0]);
	close(pfd[1]);
	syslog(LOG_ERR, "fork %s: %m", path);
	warnx("internal resource failure");
	break;
    case 0:			/* child process */
	close(pfd[0]);
	if (pfd[1] != AUTHCOMM_FD) {
	    if (dup2(pfd[1], AUTHCOMM_FD) < 0)
		err(1, "dup backchannel");
	    close(pfd[1]);
	}
	for (r = getdtablesize(); --r > AUTHCOMM_FD; )
	    close(r);
	execve(path, argv, auth_environ);
	syslog(LOG_ERR, "exec %s: %m", path);
	err(1, path);
    default:			/* parent */
	close(pfd[1]);
	if (data && nbytes)
	    write(pfd[0], data, nbytes);
	r = spooldata(pfd[0]);
	close(pfd[0]);
	if (waitpid(pid, &status, 0) < 0) {
	    syslog(LOG_ERR, "%s: waitpid: %m", path);
	    warnx("internal failure");
	    r = -1;
	} else {
	    if (r != 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0)
		r = -1;
	}
	/* kill the buffer if it is of no use */
	if (r != 0) {
	    free(spoolbuf);
	    spoolbuf = NULL;
	    spoolidx = 0;
	}
	break;
    }
    return r;
}



/*
 * auth_script()
 * Runs an authentication program with specified arguments.
 * It sets up file descriptor 3 for the program to write to;
 * it stashes the output somewhere.  The output of the program
 * consists of statements:
 *	reject [challenge|silent]
 *	authorize [root|secure]
 *	setenv <name> [<value>]
 *	remove <file>
 *
 * Terribly exciting, isn't it?
 * Output cannot exceed AUTHMAXSPOOL characters.
 */

int
auth_script(const char *path, ...)
{
    int		r;
    va_list	ap;

    va_start(ap, path);
    r = _auth_script(NULL, 0, path, ap);
    va_end(ap);
    return r;
}


int
auth_script_data(const char *data, int nbytes, const char *path, ...)
{
    int		r;
    va_list	ap;

    va_start(ap, path);
    r = _auth_script(data, nbytes, path, ap);
    va_end(ap);
    return r;
}


static void
add_rmlist(const char *file)
{
    struct rmfiles *rm;

    if ((rm = malloc(sizeof(struct rmfiles) + strlen(file) + 1)) == NULL)
	syslog(LOG_ERR, "add_rmfile malloc: %m");
    else {
	strcpy(rm->file, file);
	rm->next = rmfirst;
	rmfirst = rm;
    }
}


int
auth_scan(int okay)
{
    int	    idx = 0;
    char    *line;

    while ((line = nextline(&idx)) != NULL) {
	if (!strncasecmp(line, BI_REJECT, sizeof(BI_REJECT)-1)) {
	    line += sizeof(BI_REJECT) - 1;
	    while (*line && isspace(*line))
		++line;
	    if (*line) {
		if (!strcasecmp(line, "silent"))
		    return AUTH_SILENT;
		if (!strcasecmp(line, "challenge"))
		    return AUTH_CHALLENGE;
	    }
	    return 0;
	} else if (!strncasecmp(line, BI_AUTH, sizeof(BI_AUTH)-1)) {
	    line += sizeof(BI_AUTH) - 1;
	    while (*line && isspace(*line))
		++line;
	    if (*line == '\0')
		okay |= AUTH_OKAY;
	    else if (!strcasecmp(line, "root"))
		okay |= AUTH_ROOTOKAY;
	    else if (!strcasecmp(line, "secure"))
		okay |= AUTH_SECURE;
	}
	else if (!strncasecmp(line, BI_REMOVE, sizeof(BI_REMOVE)-1)) {
	    line += sizeof(BI_REMOVE) - 1;
	    while (*line && isspace(*line))
		++line;
	    if (*line)
		add_rmlist(line);
	}
    }

    return okay;
}


int
auth_setopt(const char *n, const char *v)
{
    int		    r;
    struct authopts *e;

    if ((e = malloc(sizeof(*e) + strlen(n) + strlen(v) + 1)) == NULL)
	r = -1;
    else {
	sprintf(e->opt, "%s=%s", n, v);
	e->next = optfirst;
	optfirst = e;
	r = 0;
    }
    return r;
}


void
auth_clropts(void)
{
    struct authopts *e;

    while ((e = optfirst) != NULL) {
	optfirst = e->next;
	free(e);
    }
}


void
auth_rmfiles(void)
{
    struct rmfiles  *rm;

    while ((rm = rmfirst) != NULL) {
	unlink(rm->file);
	rmfirst = rm->next;
	free(rm);
    }
}

#endif


/*
 * auth_checknologin()
 * Checks for the existance of a nologin file in the login_cap
 * capability <lc>.  If there isn't one specified, then it checks
 * to see if this class should just ignore nologin files.  Lastly,
 * it tries to print out the default nologin file, and, if such
 * exists, it exits.
 */

void
auth_checknologin(login_cap_t *lc)
{
  char *file;

  /* Do we ignore a nologin file? */
  if (login_getcapbool(lc, "ignorenologin", 0))
    return;

  /* Note that <file> will be "" if there is no nologin capability */
  if ((file = login_getcapstr(lc, "nologin", "", NULL)) == NULL)
    exit(1);

  /*
   * *file is true IFF there was a "nologin" capability
   * Note that auth_cat() returns 1 only if the specified
   * file exists, and is readable.  E.g., /.nologin exists.
   */
  if ((*file && auth_cat(file)) || auth_cat(_PATH_NOLOGIN))
    exit(1);
}


/*
 * auth_cat()
 * Checks for the readability of <file>; if it can be opened for
 * reading, it prints it out to stdout, and then exits.  Otherwise,
 * it returns 0 (meaning no nologin file).
 */

int
auth_cat(const char *file)
{
  int fd, count;
  char buf[BUFSIZ];

  if ((fd = open(file, O_RDONLY)) < 0)
    return 0;
  while ((count = read(fd, buf, sizeof(buf))) > 0)
    (void)write(fileno(stdout), buf, count);
  close(fd);
  sleep(5);
  return 1;
}
