/*
 * Copyright (c) 2000 Markus Friedl.  All rights reserved.
 * Copyright (c) 2005 Damien Miller.  All rights reserved.
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
RCSID("$OpenBSD: misc.c,v 1.34 2005/07/08 09:26:18 dtucker Exp $");

#include "misc.h"
#include "log.h"
#include "xmalloc.h"

/* remove newline at end of string */
char *
chop(char *s)
{
	char *t = s;
	while (*t) {
		if (*t == '\n' || *t == '\r') {
			*t = '\0';
			return s;
		}
		t++;
	}
	return s;

}

/* set/unset filedescriptor to non-blocking */
int
set_nonblock(int fd)
{
	int val;

	val = fcntl(fd, F_GETFL, 0);
	if (val < 0) {
		error("fcntl(%d, F_GETFL, 0): %s", fd, strerror(errno));
		return (-1);
	}
	if (val & O_NONBLOCK) {
		debug3("fd %d is O_NONBLOCK", fd);
		return (0);
	}
	debug2("fd %d setting O_NONBLOCK", fd);
	val |= O_NONBLOCK;
	if (fcntl(fd, F_SETFL, val) == -1) {
		debug("fcntl(%d, F_SETFL, O_NONBLOCK): %s", fd,
		    strerror(errno));
		return (-1);
	}
	return (0);
}

int
unset_nonblock(int fd)
{
	int val;

	val = fcntl(fd, F_GETFL, 0);
	if (val < 0) {
		error("fcntl(%d, F_GETFL, 0): %s", fd, strerror(errno));
		return (-1);
	}
	if (!(val & O_NONBLOCK)) {
		debug3("fd %d is not O_NONBLOCK", fd);
		return (0);
	}
	debug("fd %d clearing O_NONBLOCK", fd);
	val &= ~O_NONBLOCK;
	if (fcntl(fd, F_SETFL, val) == -1) {
		debug("fcntl(%d, F_SETFL, ~O_NONBLOCK): %s",
		    fd, strerror(errno));
		return (-1);
	}
	return (0);
}

/* disable nagle on socket */
void
set_nodelay(int fd)
{
	int opt;
	socklen_t optlen;

	optlen = sizeof opt;
	if (getsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &opt, &optlen) == -1) {
		debug("getsockopt TCP_NODELAY: %.100s", strerror(errno));
		return;
	}
	if (opt == 1) {
		debug2("fd %d is TCP_NODELAY", fd);
		return;
	}
	opt = 1;
	debug2("fd %d setting TCP_NODELAY", fd);
	if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof opt) == -1)
		error("setsockopt TCP_NODELAY: %.100s", strerror(errno));
}

/* Characters considered whitespace in strsep calls. */
#define WHITESPACE " \t\r\n"

/* return next token in configuration line */
char *
strdelim(char **s)
{
	char *old;
	int wspace = 0;

	if (*s == NULL)
		return NULL;

	old = *s;

	*s = strpbrk(*s, WHITESPACE "=");
	if (*s == NULL)
		return (old);

	/* Allow only one '=' to be skipped */
	if (*s[0] == '=')
		wspace = 1;
	*s[0] = '\0';

	*s += strspn(*s + 1, WHITESPACE) + 1;
	if (*s[0] == '=' && !wspace)
		*s += strspn(*s + 1, WHITESPACE) + 1;

	return (old);
}

struct passwd *
pwcopy(struct passwd *pw)
{
	struct passwd *copy = xmalloc(sizeof(*copy));

	memset(copy, 0, sizeof(*copy));
	copy->pw_name = xstrdup(pw->pw_name);
	copy->pw_passwd = xstrdup(pw->pw_passwd);
	copy->pw_gecos = xstrdup(pw->pw_gecos);
	copy->pw_uid = pw->pw_uid;
	copy->pw_gid = pw->pw_gid;
#ifdef HAVE_PW_EXPIRE_IN_PASSWD
	copy->pw_expire = pw->pw_expire;
#endif
#ifdef HAVE_PW_CHANGE_IN_PASSWD
	copy->pw_change = pw->pw_change;
#endif
#ifdef HAVE_PW_CLASS_IN_PASSWD
	copy->pw_class = xstrdup(pw->pw_class);
#endif
	copy->pw_dir = xstrdup(pw->pw_dir);
	copy->pw_shell = xstrdup(pw->pw_shell);
	return copy;
}

/*
 * Convert ASCII string to TCP/IP port number.
 * Port must be >0 and <=65535.
 * Return 0 if invalid.
 */
int
a2port(const char *s)
{
	long port;
	char *endp;

	errno = 0;
	port = strtol(s, &endp, 0);
	if (s == endp || *endp != '\0' ||
	    (errno == ERANGE && (port == LONG_MIN || port == LONG_MAX)) ||
	    port <= 0 || port > 65535)
		return 0;

	return port;
}

#define SECONDS		1
#define MINUTES		(SECONDS * 60)
#define HOURS		(MINUTES * 60)
#define DAYS		(HOURS * 24)
#define WEEKS		(DAYS * 7)

/*
 * Convert a time string into seconds; format is
 * a sequence of:
 *      time[qualifier]
 *
 * Valid time qualifiers are:
 *      <none>  seconds
 *      s|S     seconds
 *      m|M     minutes
 *      h|H     hours
 *      d|D     days
 *      w|W     weeks
 *
 * Examples:
 *      90m     90 minutes
 *      1h30m   90 minutes
 *      2d      2 days
 *      1w      1 week
 *
 * Return -1 if time string is invalid.
 */
long
convtime(const char *s)
{
	long total, secs;
	const char *p;
	char *endp;

	errno = 0;
	total = 0;
	p = s;

	if (p == NULL || *p == '\0')
		return -1;

	while (*p) {
		secs = strtol(p, &endp, 10);
		if (p == endp ||
		    (errno == ERANGE && (secs == LONG_MIN || secs == LONG_MAX)) ||
		    secs < 0)
			return -1;

		switch (*endp++) {
		case '\0':
			endp--;
		case 's':
		case 'S':
			break;
		case 'm':
		case 'M':
			secs *= MINUTES;
			break;
		case 'h':
		case 'H':
			secs *= HOURS;
			break;
		case 'd':
		case 'D':
			secs *= DAYS;
			break;
		case 'w':
		case 'W':
			secs *= WEEKS;
			break;
		default:
			return -1;
		}
		total += secs;
		if (total < 0)
			return -1;
		p = endp;
	}

	return total;
}

/*
 * Search for next delimiter between hostnames/addresses and ports.
 * Argument may be modified (for termination).
 * Returns *cp if parsing succeeds.
 * *cp is set to the start of the next delimiter, if one was found.
 * If this is the last field, *cp is set to NULL.
 */
char *
hpdelim(char **cp)
{
	char *s, *old;

	if (cp == NULL || *cp == NULL)
		return NULL;

	old = s = *cp;
	if (*s == '[') {
		if ((s = strchr(s, ']')) == NULL)
			return NULL;
		else
			s++;
	} else if ((s = strpbrk(s, ":/")) == NULL)
		s = *cp + strlen(*cp); /* skip to end (see first case below) */

	switch (*s) {
	case '\0':
		*cp = NULL;	/* no more fields*/
		break;

	case ':':
	case '/':
		*s = '\0';	/* terminate */
		*cp = s + 1;
		break;

	default:
		return NULL;
	}

	return old;
}

char *
cleanhostname(char *host)
{
	if (*host == '[' && host[strlen(host) - 1] == ']') {
		host[strlen(host) - 1] = '\0';
		return (host + 1);
	} else
		return host;
}

char *
colon(char *cp)
{
	int flag = 0;

	if (*cp == ':')		/* Leading colon is part of file name. */
		return (0);
	if (*cp == '[')
		flag = 1;

	for (; *cp; ++cp) {
		if (*cp == '@' && *(cp+1) == '[')
			flag = 1;
		if (*cp == ']' && *(cp+1) == ':' && flag)
			return (cp+1);
		if (*cp == ':' && !flag)
			return (cp);
		if (*cp == '/')
			return (0);
	}
	return (0);
}

/* function to assist building execv() arguments */
void
addargs(arglist *args, char *fmt, ...)
{
	va_list ap;
	char buf[1024];
	u_int nalloc;

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	nalloc = args->nalloc;
	if (args->list == NULL) {
		nalloc = 32;
		args->num = 0;
	} else if (args->num+2 >= nalloc)
		nalloc *= 2;

	args->list = xrealloc(args->list, nalloc * sizeof(char *));
	args->nalloc = nalloc;
	args->list[args->num++] = xstrdup(buf);
	args->list[args->num] = NULL;
}

/*
 * Expands tildes in the file name.  Returns data allocated by xmalloc.
 * Warning: this calls getpw*.
 */
char *
tilde_expand_filename(const char *filename, uid_t uid)
{
	const char *path;
	char user[128], ret[MAXPATHLEN];
	struct passwd *pw;
	u_int len, slash;

	if (*filename != '~')
		return (xstrdup(filename));
	filename++;

	path = strchr(filename, '/');
	if (path != NULL && path > filename) {		/* ~user/path */
		slash = path - filename;
		if (slash > sizeof(user) - 1)
			fatal("tilde_expand_filename: ~username too long");
		memcpy(user, filename, slash);
		user[slash] = '\0';
		if ((pw = getpwnam(user)) == NULL)
			fatal("tilde_expand_filename: No such user %s", user);
	} else if ((pw = getpwuid(uid)) == NULL)	/* ~/path */
		fatal("tilde_expand_filename: No such uid %d", uid);

	if (strlcpy(ret, pw->pw_dir, sizeof(ret)) >= sizeof(ret))
		fatal("tilde_expand_filename: Path too long");

	/* Make sure directory has a trailing '/' */
	len = strlen(pw->pw_dir);
	if ((len == 0 || pw->pw_dir[len - 1] != '/') &&
	    strlcat(ret, "/", sizeof(ret)) >= sizeof(ret))
		fatal("tilde_expand_filename: Path too long");

	/* Skip leading '/' from specified path */
	if (path != NULL)
		filename = path + 1;
	if (strlcat(ret, filename, sizeof(ret)) >= sizeof(ret))
		fatal("tilde_expand_filename: Path too long");

	return (xstrdup(ret));
}

/*
 * Expand a string with a set of %[char] escapes. A number of escapes may be
 * specified as (char *escape_chars, char *replacement) pairs. The list must
 * be terminated by a NULL escape_char. Returns replaced string in memory
 * allocated by xmalloc.
 */
char *
percent_expand(const char *string, ...)
{
#define EXPAND_MAX_KEYS	16
	struct {
		const char *key;
		const char *repl;
	} keys[EXPAND_MAX_KEYS];
	u_int num_keys, i, j;
	char buf[4096];
	va_list ap;

	/* Gather keys */
	va_start(ap, string);
	for (num_keys = 0; num_keys < EXPAND_MAX_KEYS; num_keys++) {
		keys[num_keys].key = va_arg(ap, char *);
		if (keys[num_keys].key == NULL)
			break;
		keys[num_keys].repl = va_arg(ap, char *);
		if (keys[num_keys].repl == NULL)
			fatal("percent_expand: NULL replacement");
	}
	va_end(ap);

	if (num_keys >= EXPAND_MAX_KEYS)
		fatal("percent_expand: too many keys");

	/* Expand string */
	*buf = '\0';
	for (i = 0; *string != '\0'; string++) {
		if (*string != '%') {
 append:
			buf[i++] = *string;
			if (i >= sizeof(buf))
				fatal("percent_expand: string too long");
			buf[i] = '\0';
			continue;
		}
		string++;
		if (*string == '%')
			goto append;
		for (j = 0; j < num_keys; j++) {
			if (strchr(keys[j].key, *string) != NULL) {
				i = strlcat(buf, keys[j].repl, sizeof(buf));
				if (i >= sizeof(buf))
					fatal("percent_expand: string too long");
				break;
			}
		}
		if (j >= num_keys)
			fatal("percent_expand: unknown key %%%c", *string);
	}
	return (xstrdup(buf));
#undef EXPAND_MAX_KEYS
}

/*
 * Read an entire line from a public key file into a static buffer, discarding
 * lines that exceed the buffer size.  Returns 0 on success, -1 on failure.
 */
int
read_keyfile_line(FILE *f, const char *filename, char *buf, size_t bufsz,
   u_long *lineno)
{
	while (fgets(buf, bufsz, f) != NULL) {
		(*lineno)++;
		if (buf[strlen(buf) - 1] == '\n' || feof(f)) {
			return 0;
		} else {
			debug("%s: %s line %lu exceeds size limit", __func__,
			    filename, *lineno);
			/* discard remainder of line */
			while (fgetc(f) != '\n' && !feof(f))
				;	/* nothing */
		}
	}
	return -1;
}

char *
tohex(const u_char *d, u_int l)
{
	char b[3], *r;
	u_int i, hl;

	hl = l * 2 + 1;
	r = xmalloc(hl);
	*r = '\0';
	for (i = 0; i < l; i++) {
		snprintf(b, sizeof(b), "%02x", d[i]);
		strlcat(r, b, hl);
	}
	return (r);
}

