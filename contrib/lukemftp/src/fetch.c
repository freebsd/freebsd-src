/*	$NetBSD: fetch.c,v 1.146 2003/12/10 12:34:28 lukem Exp $	*/

/*-
 * Copyright (c) 1997-2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Luke Mewburn.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Scott Aaron Bamford.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: fetch.c,v 1.146 2003/12/10 12:34:28 lukem Exp $");
#endif /* not lint */

/*
 * FTP User Program -- Command line file retrieval
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <netinet/in.h>

#include <arpa/ftp.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <netdb.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <libutil.h>

#include "ftp_var.h"
#include "version.h"

typedef enum {
	UNKNOWN_URL_T=-1,
	HTTP_URL_T,
	FTP_URL_T,
	FILE_URL_T,
	CLASSIC_URL_T
} url_t;

void		aborthttp(int);
static int	auth_url(const char *, char **, const char *, const char *);
static void	base64_encode(const u_char *, size_t, u_char *);
static int	go_fetch(const char *);
static int	fetch_ftp(const char *);
static int	fetch_url(const char *, const char *, char *, char *);
static int	parse_url(const char *, const char *, url_t *, char **,
			    char **, char **, char **, in_port_t *, char **);
static void	url_decode(char *);

static int	redirect_loop;


#define	ABOUT_URL	"about:"	/* propaganda */
#define	FILE_URL	"file://"	/* file URL prefix */
#define	FTP_URL		"ftp://"	/* ftp URL prefix */
#define	HTTP_URL	"http://"	/* http URL prefix */


/*
 * Generate authorization response based on given authentication challenge.
 * Returns -1 if an error occurred, otherwise 0.
 * Sets response to a malloc(3)ed string; caller should free.
 */
static int
auth_url(const char *challenge, char **response, const char *guser,
	const char *gpass)
{
	char		*cp, *ep, *clear, *line, *realm, *scheme;
	char		 user[BUFSIZ], *pass;
	int		 rval;
	size_t		 len, clen, rlen;

	*response = NULL;
	clear = realm = scheme = NULL;
	rval = -1;
	line = xstrdup(challenge);
	cp = line;

	if (debug)
		fprintf(ttyout, "auth_url: challenge `%s'\n", challenge);

	scheme = strsep(&cp, " ");
#define	SCHEME_BASIC "Basic"
	if (strncasecmp(scheme, SCHEME_BASIC, sizeof(SCHEME_BASIC) - 1) != 0) {
		warnx("Unsupported WWW Authentication challenge - `%s'",
		    challenge);
		goto cleanup_auth_url;
	}
	cp += strspn(cp, " ");

#define	REALM "realm=\""
	if (strncasecmp(cp, REALM, sizeof(REALM) - 1) == 0)
		cp += sizeof(REALM) - 1;
	else {
		warnx("Unsupported WWW Authentication challenge - `%s'",
		    challenge);
		goto cleanup_auth_url;
	}
	if ((ep = strchr(cp, '\"')) != NULL) {
		size_t len = ep - cp;

		realm = (char *)xmalloc(len + 1);
		(void)strlcpy(realm, cp, len + 1);
	} else {
		warnx("Unsupported WWW Authentication challenge - `%s'",
		    challenge);
		goto cleanup_auth_url;
	}

	if (guser != NULL)
		(void)strlcpy(user, guser, sizeof(user));
	else {
		fprintf(ttyout, "Username for `%s': ", realm);
		(void)fflush(ttyout);
		if (fgets(user, sizeof(user) - 1, stdin) == NULL) {
			clearerr(stdin);
			goto cleanup_auth_url;
		}
		user[strlen(user) - 1] = '\0';
	}
	if (gpass != NULL)
		pass = (char *)gpass;
	else
		pass = getpass("Password: ");

	clen = strlen(user) + strlen(pass) + 2;	/* user + ":" + pass + "\0" */
	clear = (char *)xmalloc(clen);
	(void)strlcpy(clear, user, clen);
	(void)strlcat(clear, ":", clen);
	(void)strlcat(clear, pass, clen);
	if (gpass == NULL)
		memset(pass, 0, strlen(pass));

						/* scheme + " " + enc + "\0" */
	rlen = strlen(scheme) + 1 + (clen + 2) * 4 / 3 + 1;
	*response = (char *)xmalloc(rlen);
	(void)strlcpy(*response, scheme, rlen);
	len = strlcat(*response, " ", rlen);
	base64_encode(clear, clen, (u_char *)*response + len);
	memset(clear, 0, clen);
	rval = 0;

 cleanup_auth_url:
	FREEPTR(clear);
	FREEPTR(line);
	FREEPTR(realm);
	return (rval);
}

/*
 * Encode len bytes starting at clear using base64 encoding into encoded,
 * which should be at least ((len + 2) * 4 / 3 + 1) in size.
 */
static void
base64_encode(const u_char *clear, size_t len, u_char *encoded)
{
	static const u_char enc[] =
	    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	u_char	*cp;
	int	 i;

	cp = encoded;
	for (i = 0; i < len; i += 3) {
		*(cp++) = enc[((clear[i + 0] >> 2))];
		*(cp++) = enc[((clear[i + 0] << 4) & 0x30)
			    | ((clear[i + 1] >> 4) & 0x0f)];
		*(cp++) = enc[((clear[i + 1] << 2) & 0x3c)
			    | ((clear[i + 2] >> 6) & 0x03)];
		*(cp++) = enc[((clear[i + 2]     ) & 0x3f)];
	}
	*cp = '\0';
	while (i-- > len)
		*(--cp) = '=';
}

/*
 * Decode %xx escapes in given string, `in-place'.
 */
static void
url_decode(char *url)
{
	unsigned char *p, *q;

	if (EMPTYSTRING(url))
		return;
	p = q = (unsigned char *)url;

#define	HEXTOINT(x) (x - (isdigit(x) ? '0' : (islower(x) ? 'a' : 'A') - 10))
	while (*p) {
		if (p[0] == '%'
		    && p[1] && isxdigit((unsigned char)p[1])
		    && p[2] && isxdigit((unsigned char)p[2])) {
			*q++ = HEXTOINT(p[1]) * 16 + HEXTOINT(p[2]);
			p+=3;
		} else
			*q++ = *p++;
	}
	*q = '\0';
}


/*
 * Parse URL of form:
 *	<type>://[<user>[:<password>]@]<host>[:<port>][/<path>]
 * Returns -1 if a parse error occurred, otherwise 0.
 * It's the caller's responsibility to url_decode() the returned
 * user, pass and path.
 *
 * Sets type to url_t, each of the given char ** pointers to a
 * malloc(3)ed strings of the relevant section, and port to
 * the number given, or ftpport if ftp://, or httpport if http://.
 *
 * If <host> is surrounded by `[' and ']', it's parsed as an
 * IPv6 address (as per RFC 2732).
 *
 * XXX: this is not totally RFC 1738 compliant; <path> will have the
 * leading `/' unless it's an ftp:// URL, as this makes things easier
 * for file:// and http:// URLs. ftp:// URLs have the `/' between the
 * host and the URL-path removed, but any additional leading slashes
 * in the URL-path are retained (because they imply that we should
 * later do "CWD" with a null argument).
 *
 * Examples:
 *	 input URL			 output path
 *	 ---------			 -----------
 *	"ftp://host"			NULL
 *	"http://host/"			NULL
 *	"file://host/dir/file"		"dir/file"
 *	"ftp://host/"			""
 *	"ftp://host//"			NULL
 *	"ftp://host//dir/file"		"/dir/file"
 */
static int
parse_url(const char *url, const char *desc, url_t *type,
		char **user, char **pass, char **host, char **port,
		in_port_t *portnum, char **path)
{
	const char	*origurl;
	char		*cp, *ep, *thost, *tport;
	size_t		 len;

	if (url == NULL || desc == NULL || type == NULL || user == NULL
	    || pass == NULL || host == NULL || port == NULL || portnum == NULL
	    || path == NULL)
		errx(1, "parse_url: invoked with NULL argument!");

	origurl = url;
	*type = UNKNOWN_URL_T;
	*user = *pass = *host = *port = *path = NULL;
	*portnum = 0;
	tport = NULL;

	if (strncasecmp(url, HTTP_URL, sizeof(HTTP_URL) - 1) == 0) {
		url += sizeof(HTTP_URL) - 1;
		*type = HTTP_URL_T;
		*portnum = HTTP_PORT;
		tport = httpport;
	} else if (strncasecmp(url, FTP_URL, sizeof(FTP_URL) - 1) == 0) {
		url += sizeof(FTP_URL) - 1;
		*type = FTP_URL_T;
		*portnum = FTP_PORT;
		tport = ftpport;
	} else if (strncasecmp(url, FILE_URL, sizeof(FILE_URL) - 1) == 0) {
		url += sizeof(FILE_URL) - 1;
		*type = FILE_URL_T;
	} else {
		warnx("Invalid %s `%s'", desc, url);
 cleanup_parse_url:
		FREEPTR(*user);
		FREEPTR(*pass);
		FREEPTR(*host);
		FREEPTR(*port);
		FREEPTR(*path);
		return (-1);
	}

	if (*url == '\0')
		return (0);

			/* find [user[:pass]@]host[:port] */
	ep = strchr(url, '/');
	if (ep == NULL)
		thost = xstrdup(url);
	else {
		len = ep - url;
		thost = (char *)xmalloc(len + 1);
		(void)strlcpy(thost, url, len + 1);
		if (*type == FTP_URL_T)	/* skip first / for ftp URLs */
			ep++;
		*path = xstrdup(ep);
	}

	cp = strchr(thost, '@');	/* look for user[:pass]@ in URLs */
	if (cp != NULL) {
		if (*type == FTP_URL_T)
			anonftp = 0;	/* disable anonftp */
		*user = thost;
		*cp = '\0';
		thost = xstrdup(cp + 1);
		cp = strchr(*user, ':');
		if (cp != NULL) {
			*cp = '\0';
			*pass = xstrdup(cp + 1);
		}
	}

#ifdef INET6
			/*
			 * Check if thost is an encoded IPv6 address, as per
			 * RFC 2732:
			 *	`[' ipv6-address ']'
			 */
	if (*thost == '[') {
		cp = thost + 1;
		if ((ep = strchr(cp, ']')) == NULL ||
		    (ep[1] != '\0' && ep[1] != ':')) {
			warnx("Invalid address `%s' in %s `%s'",
			    thost, desc, origurl);
			goto cleanup_parse_url;
		}
		len = ep - cp;		/* change `[xyz]' -> `xyz' */
		memmove(thost, thost + 1, len);
		thost[len] = '\0';
		if (! isipv6addr(thost)) {
			warnx("Invalid IPv6 address `%s' in %s `%s'",
			    thost, desc, origurl);
			goto cleanup_parse_url;
		}
		cp = ep + 1;
		if (*cp == ':')
			cp++;
		else
			cp = NULL;
	} else
#endif /* INET6 */
	    if ((cp = strchr(thost, ':')) != NULL)
		*cp++ =  '\0';
	*host = thost;

			/* look for [:port] */
	if (cp != NULL) {
		long	nport;

		nport = parseport(cp, -1);
		if (nport == -1) {
			warnx("Unknown port `%s' in %s `%s'",
			    cp, desc, origurl);
			goto cleanup_parse_url;
		}
		*portnum = nport;
		tport = cp;
	}

	if (tport != NULL)
		*port = xstrdup(tport);
	if (*path == NULL)
		*path = xstrdup("/");

	if (debug)
		fprintf(ttyout,
		    "parse_url: user `%s' pass `%s' host %s port %s(%d) "
		    "path `%s'\n",
		    *user ? *user : "<null>", *pass ? *pass : "<null>",
		    *host ? *host : "<null>", *port ? *port : "<null>",
		    *portnum ? *portnum : -1, *path ? *path : "<null>");

	return (0);
}

sigjmp_buf	httpabort;

/*
 * Retrieve URL, via a proxy if necessary, using HTTP.
 * If proxyenv is set, use that for the proxy, otherwise try ftp_proxy or
 * http_proxy as appropriate.
 * Supports HTTP redirects.
 * Returns 1 on failure, 0 on completed xfer, -1 if ftp connection
 * is still open (e.g, ftp xfer with trailing /)
 */
static int
fetch_url(const char *url, const char *proxyenv, char *proxyauth, char *wwwauth)
{
	struct addrinfo		hints, *res, *res0 = NULL;
	int			error;
	char			hbuf[NI_MAXHOST];
	volatile sigfunc	oldintr, oldintp;
	volatile int		s;
	struct stat		sb;
	int			ischunked, isproxy, rval, hcode;
	size_t			len;
	static size_t		bufsize;
	static char		*xferbuf;
	char			*cp, *ep, *buf, *savefile;
	char			*auth, *location, *message;
	char			*user, *pass, *host, *port, *path, *decodedpath;
	char			*puser, *ppass, *useragent;
	off_t			hashbytes, rangestart, rangeend, entitylen;
	int			 (*closefunc)(FILE *);
	FILE			*fin, *fout;
	time_t			mtime;
	url_t			urltype;
	in_port_t		portnum;

	oldintr = oldintp = NULL;
	closefunc = NULL;
	fin = fout = NULL;
	s = -1;
	buf = savefile = NULL;
	auth = location = message = NULL;
	ischunked = isproxy = hcode = 0;
	rval = 1;
	user = pass = host = path = decodedpath = puser = ppass = NULL;

#ifdef __GNUC__			/* shut up gcc warnings */
	(void)&closefunc;
	(void)&fin;
	(void)&fout;
	(void)&buf;
	(void)&savefile;
	(void)&rval;
	(void)&isproxy;
	(void)&hcode;
	(void)&ischunked;
	(void)&message;
	(void)&location;
	(void)&auth;
	(void)&decodedpath;
#endif

	if (parse_url(url, "URL", &urltype, &user, &pass, &host, &port,
	    &portnum, &path) == -1)
		goto cleanup_fetch_url;

	if (urltype == FILE_URL_T && ! EMPTYSTRING(host)
	    && strcasecmp(host, "localhost") != 0) {
		warnx("No support for non local file URL `%s'", url);
		goto cleanup_fetch_url;
	}

	if (EMPTYSTRING(path)) {
		if (urltype == FTP_URL_T) {
			rval = fetch_ftp(url);
			goto cleanup_fetch_url;
		}
		if (urltype != HTTP_URL_T || outfile == NULL)  {
			warnx("Invalid URL (no file after host) `%s'", url);
			goto cleanup_fetch_url;
		}
	}

	decodedpath = xstrdup(path);
	url_decode(decodedpath);

	if (outfile)
		savefile = xstrdup(outfile);
	else {
		cp = strrchr(decodedpath, '/');		/* find savefile */
		if (cp != NULL)
			savefile = xstrdup(cp + 1);
		else
			savefile = xstrdup(decodedpath);
	}
	if (EMPTYSTRING(savefile)) {
		if (urltype == FTP_URL_T) {
			rval = fetch_ftp(url);
			goto cleanup_fetch_url;
		}
		warnx("no file after directory (you must specify an "
		    "output file) `%s'", url);
		goto cleanup_fetch_url;
	} else {
		if (debug)
			fprintf(ttyout, "got savefile as `%s'\n", savefile);
	}

	restart_point = 0;
	filesize = -1;
	rangestart = rangeend = entitylen = -1;
	mtime = -1;
	if (restartautofetch) {
		if (strcmp(savefile, "-") != 0 && *savefile != '|' &&
		    stat(savefile, &sb) == 0)
			restart_point = sb.st_size;
	}
	if (urltype == FILE_URL_T) {		/* file:// URLs */
		direction = "copied";
		fin = fopen(decodedpath, "r");
		if (fin == NULL) {
			warn("Cannot open file `%s'", decodedpath);
			goto cleanup_fetch_url;
		}
		if (fstat(fileno(fin), &sb) == 0) {
			mtime = sb.st_mtime;
			filesize = sb.st_size;
		}
		if (restart_point) {
			if (lseek(fileno(fin), restart_point, SEEK_SET) < 0) {
				warn("Can't lseek to restart `%s'",
				    decodedpath);
				goto cleanup_fetch_url;
			}
		}
		if (verbose) {
			fprintf(ttyout, "Copying %s", decodedpath);
			if (restart_point)
				fprintf(ttyout, " (restarting at " LLF ")",
				    (LLT)restart_point);
			fputs("\n", ttyout);
		}
	} else {				/* ftp:// or http:// URLs */
		char *leading;
		int hasleading;

		if (proxyenv == NULL) {
			if (urltype == HTTP_URL_T)
				proxyenv = getoptionvalue("http_proxy");
			else if (urltype == FTP_URL_T)
				proxyenv = getoptionvalue("ftp_proxy");
		}
		direction = "retrieved";
		if (! EMPTYSTRING(proxyenv)) {			/* use proxy */
			url_t purltype;
			char *phost, *ppath;
			char *pport, *no_proxy;

			isproxy = 1;

				/* check URL against list of no_proxied sites */
			no_proxy = getoptionvalue("no_proxy");
			if (! EMPTYSTRING(no_proxy)) {
				char *np, *np_copy;
				long np_port;
				size_t hlen, plen;

				np_copy = xstrdup(no_proxy);
				hlen = strlen(host);
				while ((cp = strsep(&np_copy, " ,")) != NULL) {
					if (*cp == '\0')
						continue;
					if ((np = strrchr(cp, ':')) != NULL) {
						*np = '\0';
						np_port =
						    strtol(np + 1, &ep, 10);
						if (*ep != '\0')
							continue;
						if (np_port != portnum)
							continue;
					}
					plen = strlen(cp);
					if (hlen < plen)
						continue;
					if (strncasecmp(host + hlen - plen,
					    cp, plen) == 0) {
						isproxy = 0;
						break;
					}
				}
				FREEPTR(np_copy);
				if (isproxy == 0 && urltype == FTP_URL_T) {
					rval = fetch_ftp(url);
					goto cleanup_fetch_url;
				}
			}

			if (isproxy) {
				if (parse_url(proxyenv, "proxy URL", &purltype,
				    &puser, &ppass, &phost, &pport, &portnum,
				    &ppath) == -1)
					goto cleanup_fetch_url;

				if ((purltype != HTTP_URL_T
				     && purltype != FTP_URL_T) ||
				    EMPTYSTRING(phost) ||
				    (! EMPTYSTRING(ppath)
				     && strcmp(ppath, "/") != 0)) {
					warnx("Malformed proxy URL `%s'",
					    proxyenv);
					FREEPTR(phost);
					FREEPTR(pport);
					FREEPTR(ppath);
					goto cleanup_fetch_url;
				}
				if (isipv6addr(host) &&
				    strchr(host, '%') != NULL) {
					warnx(
"Scoped address notation `%s' disallowed via web proxy",
					    host);
					FREEPTR(phost);
					FREEPTR(pport);
					FREEPTR(ppath);
					goto cleanup_fetch_url;
				}

				FREEPTR(host);
				host = phost;
				FREEPTR(port);
				port = pport;
				FREEPTR(path);
				path = xstrdup(url);
				FREEPTR(ppath);
			}
		} /* ! EMPTYSTRING(proxyenv) */

		memset(&hints, 0, sizeof(hints));
		hints.ai_flags = 0;
		hints.ai_family = family;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = 0;
		error = getaddrinfo(host, NULL, &hints, &res0);
		if (error) {
			warnx("%s", gai_strerror(error));
			goto cleanup_fetch_url;
		}
		if (res0->ai_canonname)
			host = res0->ai_canonname;

		s = -1;
		for (res = res0; res; res = res->ai_next) {
			/*
			 * see comment in hookup()
			 */
			ai_unmapped(res);
			if (getnameinfo(res->ai_addr, res->ai_addrlen,
			    hbuf, sizeof(hbuf), NULL, 0, NI_NUMERICHOST) != 0)
				strlcpy(hbuf, "invalid", sizeof(hbuf));

			if (verbose && res != res0)
				fprintf(ttyout, "Trying %s...\n", hbuf);

			((struct sockaddr_in *)res->ai_addr)->sin_port =
			    htons(portnum);
			s = socket(res->ai_family, SOCK_STREAM,
			    res->ai_protocol);
			if (s < 0) {
				warn("Can't create socket");
				continue;
			}

			if (xconnect(s, res->ai_addr, res->ai_addrlen) < 0) {
				warn("Connect to address `%s'", hbuf);
				close(s);
				s = -1;
				continue;
			}

			/* success */
			break;
		}
		freeaddrinfo(res0);

		if (s < 0) {
			warn("Can't connect to %s", host);
			goto cleanup_fetch_url;
		}

		fin = fdopen(s, "r+");
		/*
		 * Construct and send the request.
		 */
		if (verbose)
			fprintf(ttyout, "Requesting %s\n", url);
		leading = "  (";
		hasleading = 0;
		if (isproxy) {
			if (verbose) {
				fprintf(ttyout, "%svia %s:%s", leading,
				    host, port);
				leading = ", ";
				hasleading++;
			}
			fprintf(fin, "GET %s HTTP/1.0\r\n", path);
			if (flushcache)
				fprintf(fin, "Pragma: no-cache\r\n");
		} else {
			fprintf(fin, "GET %s HTTP/1.1\r\n", path);
			if (strchr(host, ':')) {
				char *h, *p;

				/*
				 * strip off IPv6 scope identifier, since it is
				 * local to the node
				 */
				h = xstrdup(host);
				if (isipv6addr(h) &&
				    (p = strchr(h, '%')) != NULL) {
					*p = '\0';
				}
				fprintf(fin, "Host: [%s]", h);
				free(h);
			} else
				fprintf(fin, "Host: %s", host);
			if (portnum != HTTP_PORT)
				fprintf(fin, ":%u", portnum);
			fprintf(fin, "\r\n");
			fprintf(fin, "Accept: */*\r\n");
			fprintf(fin, "Connection: close\r\n");
			if (restart_point) {
				fputs(leading, ttyout);
				fprintf(fin, "Range: bytes=" LLF "-\r\n",
				    (LLT)restart_point);
				fprintf(ttyout, "restarting at " LLF,
				    (LLT)restart_point);
				leading = ", ";
				hasleading++;
			}
			if (flushcache)
				fprintf(fin, "Cache-Control: no-cache\r\n");
		}
		if ((useragent=getenv("FTPUSERAGENT")) != NULL) {
			fprintf(fin, "User-Agent: %s\r\n", useragent);
		} else {
			fprintf(fin, "User-Agent: %s/%s\r\n",
			    FTP_PRODUCT, FTP_VERSION);
		}
		if (wwwauth) {
			if (verbose) {
				fprintf(ttyout, "%swith authorization",
				    leading);
				leading = ", ";
				hasleading++;
			}
			fprintf(fin, "Authorization: %s\r\n", wwwauth);
		}
		if (proxyauth) {
			if (verbose) {
				fprintf(ttyout,
				    "%swith proxy authorization", leading);
				leading = ", ";
				hasleading++;
			}
			fprintf(fin, "Proxy-Authorization: %s\r\n", proxyauth);
		}
		if (verbose && hasleading)
			fputs(")\n", ttyout);
		fprintf(fin, "\r\n");
		if (fflush(fin) == EOF) {
			warn("Writing HTTP request");
			goto cleanup_fetch_url;
		}

				/* Read the response */
		if ((buf = fparseln(fin, &len, NULL, "\0\0\0", 0)) == NULL) {
			warn("Receiving HTTP reply");
			goto cleanup_fetch_url;
		}
		while (len > 0 && (buf[len-1] == '\r' || buf[len-1] == '\n'))
			buf[--len] = '\0';
		if (debug)
			fprintf(ttyout, "received `%s'\n", buf);

				/* Determine HTTP response code */
		cp = strchr(buf, ' ');
		if (cp == NULL)
			goto improper;
		else
			cp++;
		hcode = strtol(cp, &ep, 10);
		if (*ep != '\0' && !isspace((unsigned char)*ep))
			goto improper;
		message = xstrdup(cp);

				/* Read the rest of the header. */
		FREEPTR(buf);
		while (1) {
			if ((buf = fparseln(fin, &len, NULL, "\0\0\0", 0))
			    == NULL) {
				warn("Receiving HTTP reply");
				goto cleanup_fetch_url;
			}
			while (len > 0 &&
			    (buf[len-1] == '\r' || buf[len-1] == '\n'))
				buf[--len] = '\0';
			if (len == 0)
				break;
			if (debug)
				fprintf(ttyout, "received `%s'\n", buf);

				/* Look for some headers */
			cp = buf;

#define	CONTENTLEN "Content-Length: "
			if (strncasecmp(cp, CONTENTLEN,
					sizeof(CONTENTLEN) - 1) == 0) {
				cp += sizeof(CONTENTLEN) - 1;
				filesize = STRTOLL(cp, &ep, 10);
				if (filesize < 0 || *ep != '\0')
					goto improper;
				if (debug)
					fprintf(ttyout,
					    "parsed len as: " LLF "\n",
					    (LLT)filesize);

#define CONTENTRANGE "Content-Range: bytes "
			} else if (strncasecmp(cp, CONTENTRANGE,
					sizeof(CONTENTRANGE) - 1) == 0) {
				cp += sizeof(CONTENTRANGE) - 1;
				if (*cp == '*') {
					ep = cp + 1;
				}
				else {
					rangestart = STRTOLL(cp, &ep, 10);
					if (rangestart < 0 || *ep != '-')
						goto improper;
					cp = ep + 1;
					rangeend = STRTOLL(cp, &ep, 10);
					if (rangeend < 0 || rangeend < rangestart)
						goto improper;
				}
				if (*ep != '/')
					goto improper;
				cp = ep + 1;
				if (*cp == '*') {
					ep = cp + 1;
				}
				else {
					entitylen = STRTOLL(cp, &ep, 10);
					if (entitylen < 0)
						goto improper;
				}
				if (*ep != '\0')
					goto improper;

				if (debug) {
					fprintf(ttyout, "parsed range as: ");
					if (rangestart == -1)
						fprintf(ttyout, "*");
					else
						fprintf(ttyout, LLF "-" LLF,
						    (LLT)rangestart,
						    (LLT)rangeend);
					fprintf(ttyout, "/" LLF "\n", (LLT)entitylen);
				}
				if (! restart_point) {
					warnx(
				    "Received unexpected Content-Range header");
					goto cleanup_fetch_url;
				}

#define	LASTMOD "Last-Modified: "
			} else if (strncasecmp(cp, LASTMOD,
						sizeof(LASTMOD) - 1) == 0) {
				struct tm parsed;
				char *t;

				cp += sizeof(LASTMOD) - 1;
							/* RFC 1123 */
				if ((t = strptime(cp,
						"%a, %d %b %Y %H:%M:%S GMT",
						&parsed))
							/* RFC 850 */
				    || (t = strptime(cp,
						"%a, %d-%b-%y %H:%M:%S GMT",
						&parsed))
							/* asctime */
				    || (t = strptime(cp,
						"%a, %b %d %H:%M:%S %Y",
						&parsed))) {
					parsed.tm_isdst = -1;
					if (*t == '\0')
						mtime = timegm(&parsed);
					if (debug && mtime != -1) {
						fprintf(ttyout,
						    "parsed date as: %s",
						    ctime(&mtime));
					}
				}

#define	LOCATION "Location: "
			} else if (strncasecmp(cp, LOCATION,
						sizeof(LOCATION) - 1) == 0) {
				cp += sizeof(LOCATION) - 1;
				location = xstrdup(cp);
				if (debug)
					fprintf(ttyout,
					    "parsed location as: %s\n", cp);

#define	TRANSENC "Transfer-Encoding: "
			} else if (strncasecmp(cp, TRANSENC,
						sizeof(TRANSENC) - 1) == 0) {
				cp += sizeof(TRANSENC) - 1;
				if (strcasecmp(cp, "binary") == 0) {
					warnx(
			"Bogus transfer encoding - `%s' (fetching anyway)",
					    cp);
					continue;
				}
				if (strcasecmp(cp, "chunked") != 0) {
					warnx(
				    "Unsupported transfer encoding - `%s'",
					    cp);
					goto cleanup_fetch_url;
				}
				ischunked++;
				if (debug)
					fprintf(ttyout,
					    "using chunked encoding\n");

#define	PROXYAUTH "Proxy-Authenticate: "
			} else if (strncasecmp(cp, PROXYAUTH,
						sizeof(PROXYAUTH) - 1) == 0) {
				cp += sizeof(PROXYAUTH) - 1;
				FREEPTR(auth);
				auth = xstrdup(cp);
				if (debug)
					fprintf(ttyout,
					    "parsed proxy-auth as: %s\n", cp);

#define	WWWAUTH	"WWW-Authenticate: "
			} else if (strncasecmp(cp, WWWAUTH,
			    sizeof(WWWAUTH) - 1) == 0) {
				cp += sizeof(WWWAUTH) - 1;
				FREEPTR(auth);
				auth = xstrdup(cp);
				if (debug)
					fprintf(ttyout,
					    "parsed www-auth as: %s\n", cp);

			}

		}
				/* finished parsing header */
		FREEPTR(buf);

		switch (hcode) {
		case 200:
			break;
		case 206:
			if (! restart_point) {
				warnx("Not expecting partial content header");
				goto cleanup_fetch_url;
			}
			break;
		case 300:
		case 301:
		case 302:
		case 303:
		case 305:
			if (EMPTYSTRING(location)) {
				warnx(
				"No redirection Location provided by server");
				goto cleanup_fetch_url;
			}
			if (redirect_loop++ > 5) {
				warnx("Too many redirections requested");
				goto cleanup_fetch_url;
			}
			if (hcode == 305) {
				if (verbose)
					fprintf(ttyout, "Redirected via %s\n",
					    location);
				rval = fetch_url(url, location,
				    proxyauth, wwwauth);
			} else {
				if (verbose)
					fprintf(ttyout, "Redirected to %s\n",
					    location);
				rval = go_fetch(location);
			}
			goto cleanup_fetch_url;
		case 401:
		case 407:
		    {
			char **authp;
			char *auser, *apass;

			fprintf(ttyout, "%s\n", message);
			if (EMPTYSTRING(auth)) {
				warnx(
			    "No authentication challenge provided by server");
				goto cleanup_fetch_url;
			}
			if (hcode == 401) {
				authp = &wwwauth;
				auser = user;
				apass = pass;
			} else {
				authp = &proxyauth;
				auser = puser;
				apass = ppass;
			}
			if (*authp != NULL) {
				char reply[10];

				fprintf(ttyout,
				    "Authorization failed. Retry (y/n)? ");
				if (fgets(reply, sizeof(reply), stdin)
				    == NULL) {
					clearerr(stdin);
					goto cleanup_fetch_url;
				} else {
					if (tolower(reply[0]) != 'y')
						goto cleanup_fetch_url;
				}
				auser = NULL;
				apass = NULL;
			}
			if (auth_url(auth, authp, auser, apass) == 0) {
				rval = fetch_url(url, proxyenv,
				    proxyauth, wwwauth);
				memset(*authp, 0, strlen(*authp));
				FREEPTR(*authp);
			}
			goto cleanup_fetch_url;
		    }
		default:
			if (message)
				warnx("Error retrieving file - `%s'", message);
			else
				warnx("Unknown error retrieving file");
			goto cleanup_fetch_url;
		}
	}		/* end of ftp:// or http:// specific setup */

			/* Open the output file. */
	if (strcmp(savefile, "-") == 0) {
		fout = stdout;
	} else if (*savefile == '|') {
		oldintp = xsignal(SIGPIPE, SIG_IGN);
		fout = popen(savefile + 1, "w");
		if (fout == NULL) {
			warn("Can't run `%s'", savefile + 1);
			goto cleanup_fetch_url;
		}
		closefunc = pclose;
	} else {
		if ((rangeend != -1 && rangeend <= restart_point) ||
		    (rangestart == -1 && filesize != -1 && filesize <= restart_point)) {
			/* already done */
			if (verbose)
				fprintf(ttyout, "already done\n");
			rval = 0;
			goto cleanup_fetch_url;
		}
		if (restart_point && rangestart != -1) {
			if (entitylen != -1)
				filesize = entitylen;
			if (rangestart != restart_point) {
				warnx(
				    "Size of `%s' differs from save file `%s'",
				    url, savefile);
				goto cleanup_fetch_url;
			}
			fout = fopen(savefile, "a");
		} else
			fout = fopen(savefile, "w");
		if (fout == NULL) {
			warn("Can't open `%s'", savefile);
			goto cleanup_fetch_url;
		}
		closefunc = fclose;
	}

			/* Trap signals */
	if (sigsetjmp(httpabort, 1))
		goto cleanup_fetch_url;
	(void)xsignal(SIGQUIT, psummary);
	oldintr = xsignal(SIGINT, aborthttp);

	if (rcvbuf_size > bufsize) {
		if (xferbuf)
			(void)free(xferbuf);
		bufsize = rcvbuf_size;
		xferbuf = xmalloc(bufsize);
	}

	bytes = 0;
	hashbytes = mark;
	progressmeter(-1);

			/* Finally, suck down the file. */
	do {
		long chunksize;

		chunksize = 0;
					/* read chunksize */
		if (ischunked) {
			if (fgets(xferbuf, bufsize, fin) == NULL) {
				warnx("Unexpected EOF reading chunksize");
				goto cleanup_fetch_url;
			}
			chunksize = strtol(xferbuf, &ep, 16);

				/*
				 * XXX:	Work around bug in Apache 1.3.9 and
				 *	1.3.11, which incorrectly put trailing
				 *	space after the chunksize.
				 */
			while (*ep == ' ')
				ep++;

			if (strcmp(ep, "\r\n") != 0) {
				warnx("Unexpected data following chunksize");
				goto cleanup_fetch_url;
			}
			if (debug)
				fprintf(ttyout, "got chunksize of " LLF "\n",
				    (LLT)chunksize);
			if (chunksize == 0)
				break;
		}
					/* transfer file or chunk */
		while (1) {
			struct timeval then, now, td;
			off_t bufrem;

			if (rate_get)
				(void)gettimeofday(&then, NULL);
			bufrem = rate_get ? rate_get : bufsize;
			if (ischunked)
				bufrem = MIN(chunksize, bufrem);
			while (bufrem > 0) {
				len = fread(xferbuf, sizeof(char),
				    MIN(bufsize, bufrem), fin);
				if (len <= 0)
					goto chunkdone;
				bytes += len;
				bufrem -= len;
				if (fwrite(xferbuf, sizeof(char), len, fout)
				    != len) {
					warn("Writing `%s'", savefile);
					goto cleanup_fetch_url;
				}
				if (hash && !progress) {
					while (bytes >= hashbytes) {
						(void)putc('#', ttyout);
						hashbytes += mark;
					}
					(void)fflush(ttyout);
				}
				if (ischunked) {
					chunksize -= len;
					if (chunksize <= 0)
						break;
				}
			}
			if (rate_get) {
				while (1) {
					(void)gettimeofday(&now, NULL);
					timersub(&now, &then, &td);
					if (td.tv_sec > 0)
						break;
					usleep(1000000 - td.tv_usec);
				}
			}
			if (ischunked && chunksize <= 0)
				break;
		}
					/* read CRLF after chunk*/
 chunkdone:
		if (ischunked) {
			if (fgets(xferbuf, bufsize, fin) == NULL)
				break;
			if (strcmp(xferbuf, "\r\n") != 0) {
				warnx("Unexpected data following chunk");
				goto cleanup_fetch_url;
			}
		}
	} while (ischunked);
	if (hash && !progress && bytes > 0) {
		if (bytes < mark)
			(void)putc('#', ttyout);
		(void)putc('\n', ttyout);
	}
	if (ferror(fin)) {
		warn("Reading file");
		goto cleanup_fetch_url;
	}
	progressmeter(1);
	(void)fflush(fout);
	if (closefunc == fclose && mtime != -1) {
		struct timeval tval[2];

		(void)gettimeofday(&tval[0], NULL);
		tval[1].tv_sec = mtime;
		tval[1].tv_usec = 0;
		(*closefunc)(fout);
		fout = NULL;

		if (utimes(savefile, tval) == -1) {
			fprintf(ttyout,
			    "Can't change modification time to %s",
			    asctime(localtime(&mtime)));
		}
	}
	if (bytes > 0)
		ptransfer(0);
	bytes = 0;

	rval = 0;
	goto cleanup_fetch_url;

 improper:
	warnx("Improper response from `%s'", host);

 cleanup_fetch_url:
	if (oldintr)
		(void)xsignal(SIGINT, oldintr);
	if (oldintp)
		(void)xsignal(SIGPIPE, oldintp);
	if (fin != NULL)
		fclose(fin);
	else if (s != -1)
		close(s);
	if (closefunc != NULL && fout != NULL)
		(*closefunc)(fout);
	FREEPTR(savefile);
	FREEPTR(user);
	FREEPTR(pass);
	FREEPTR(host);
	FREEPTR(port);
	FREEPTR(path);
	FREEPTR(decodedpath);
	FREEPTR(puser);
	FREEPTR(ppass);
	FREEPTR(buf);
	FREEPTR(auth);
	FREEPTR(location);
	FREEPTR(message);
	return (rval);
}

/*
 * Abort a HTTP retrieval
 */
void
aborthttp(int notused)
{
	char msgbuf[100];
	int len;

	alarmtimer(0);
	len = strlcpy(msgbuf, "\nHTTP fetch aborted.\n", sizeof(msgbuf));
	write(fileno(ttyout), msgbuf, len);
	siglongjmp(httpabort, 1);
}

/*
 * Retrieve ftp URL or classic ftp argument using FTP.
 * Returns 1 on failure, 0 on completed xfer, -1 if ftp connection
 * is still open (e.g, ftp xfer with trailing /)
 */
static int
fetch_ftp(const char *url)
{
	char		*cp, *xargv[5], rempath[MAXPATHLEN];
	char		*host, *path, *dir, *file, *user, *pass;
	char		*port;
	int		 dirhasglob, filehasglob, oautologin, rval, type, xargc;
	in_port_t	 portnum;
	url_t		 urltype;

	host = path = dir = file = user = pass = NULL;
	port = NULL;
	rval = 1;
	type = TYPE_I;

	if (strncasecmp(url, FTP_URL, sizeof(FTP_URL) - 1) == 0) {
		if ((parse_url(url, "URL", &urltype, &user, &pass,
		    &host, &port, &portnum, &path) == -1) ||
		    (user != NULL && *user == '\0') ||
		    EMPTYSTRING(host)) {
			warnx("Invalid URL `%s'", url);
			goto cleanup_fetch_ftp;
		}
		url_decode(user);
		url_decode(pass);
		/*
		 * Note: Don't url_decode(path) here.  We need to keep the
		 * distinction between "/" and "%2F" until later.
		 */

					/* check for trailing ';type=[aid]' */
		if (! EMPTYSTRING(path) && (cp = strrchr(path, ';')) != NULL) {
			if (strcasecmp(cp, ";type=a") == 0)
				type = TYPE_A;
			else if (strcasecmp(cp, ";type=i") == 0)
				type = TYPE_I;
			else if (strcasecmp(cp, ";type=d") == 0) {
				warnx(
			    "Directory listing via a URL is not supported");
				goto cleanup_fetch_ftp;
			} else {
				warnx("Invalid suffix `%s' in URL `%s'", cp,
				    url);
				goto cleanup_fetch_ftp;
			}
			*cp = 0;
		}
	} else {			/* classic style `[user@]host:[file]' */
		urltype = CLASSIC_URL_T;
		host = xstrdup(url);
		cp = strchr(host, '@');
		if (cp != NULL) {
			*cp = '\0';
			user = host;
			anonftp = 0;	/* disable anonftp */
			host = xstrdup(cp + 1);
		}
		cp = strchr(host, ':');
		if (cp != NULL) {
			*cp = '\0';
			path = xstrdup(cp + 1);
		}
	}
	if (EMPTYSTRING(host))
		goto cleanup_fetch_ftp;

			/* Extract the file and (if present) directory name. */
	dir = path;
	if (! EMPTYSTRING(dir)) {
		/*
		 * If we are dealing with classic `[user@]host:[path]' syntax,
		 * then a path of the form `/file' (resulting from input of the
		 * form `host:/file') means that we should do "CWD /" before
		 * retrieving the file.  So we set dir="/" and file="file".
		 *
		 * But if we are dealing with URLs like `ftp://host/path' then
		 * a path of the form `/file' (resulting from a URL of the form
		 * `ftp://host//file') means that we should do `CWD ' (with an
		 * empty argument) before retrieving the file.  So we set
		 * dir="" and file="file".
		 *
		 * If the path does not contain / at all, we set dir=NULL.
		 * (We get a path without any slashes if we are dealing with
		 * classic `[user@]host:[file]' or URL `ftp://host/file'.)
		 *
		 * In all other cases, we set dir to a string that does not
		 * include the final '/' that separates the dir part from the
		 * file part of the path.  (This will be the empty string if
		 * and only if we are dealing with a path of the form `/file'
		 * resulting from an URL of the form `ftp://host//file'.)
		 */
		cp = strrchr(dir, '/');
		if (cp == dir && urltype == CLASSIC_URL_T) {
			file = cp + 1;
			dir = "/";
		} else if (cp != NULL) {
			*cp++ = '\0';
			file = cp;
		} else {
			file = dir;
			dir = NULL;
		}
	} else
		dir = NULL;
	if (urltype == FTP_URL_T && file != NULL) {
		url_decode(file);	
		/* but still don't url_decode(dir) */
	}
	if (debug)
		fprintf(ttyout,
		    "fetch_ftp: user `%s' pass `%s' host %s port %s "
		    "path `%s' dir `%s' file `%s'\n",
		    user ? user : "<null>", pass ? pass : "<null>",
		    host ? host : "<null>", port ? port : "<null>",
		    path ? path : "<null>",
		    dir ? dir : "<null>", file ? file : "<null>");

	dirhasglob = filehasglob = 0;
	if (doglob && urltype == CLASSIC_URL_T) {
		if (! EMPTYSTRING(dir) && strpbrk(dir, "*?[]{}") != NULL)
			dirhasglob = 1;
		if (! EMPTYSTRING(file) && strpbrk(file, "*?[]{}") != NULL)
			filehasglob = 1;
	}

			/* Set up the connection */
	if (connected)
		disconnect(0, NULL);
	xargv[0] = (char *)getprogname();	/* XXX discards const */
	xargv[1] = host;
	xargv[2] = NULL;
	xargc = 2;
	if (port) {
		xargv[2] = port;
		xargv[3] = NULL;
		xargc = 3;
	}
	oautologin = autologin;
		/* don't autologin in setpeer(), use ftp_login() below */
	autologin = 0;
	setpeer(xargc, xargv);
	autologin = oautologin;
	if ((connected == 0) ||
	    (connected == 1 && !ftp_login(host, user, pass))) {
		warnx("Can't connect or login to host `%s'", host);
		goto cleanup_fetch_ftp;
	}

	switch (type) {
	case TYPE_A:
		setascii(1, xargv);
		break;
	case TYPE_I:
		setbinary(1, xargv);
		break;
	default:
		errx(1, "fetch_ftp: unknown transfer type %d", type);
	}

		/*
		 * Change directories, if necessary.
		 *
		 * Note: don't use EMPTYSTRING(dir) below, because
		 * dir=="" means something different from dir==NULL.
		 */
	if (dir != NULL && !dirhasglob) {
		char *nextpart;

		/*
		 * If we are dealing with a classic `[user@]host:[path]'
		 * (urltype is CLASSIC_URL_T) then we have a raw directory
		 * name (not encoded in any way) and we can change
		 * directories in one step.
		 *
		 * If we are dealing with an `ftp://host/path' URL
		 * (urltype is FTP_URL_T), then RFC 1738 says we need to
		 * send a separate CWD command for each unescaped "/"
		 * in the path, and we have to interpret %hex escaping
		 * *after* we find the slashes.  It's possible to get
		 * empty components here, (from multiple adjacent
		 * slashes in the path) and RFC 1738 says that we should
		 * still do `CWD ' (with a null argument) in such cases.
		 *
		 * Many ftp servers don't support `CWD ', so if there's an
		 * error performing that command, bail out with a descriptive
		 * message.
		 *
		 * Examples:
		 *
		 * host:			dir="", urltype=CLASSIC_URL_T
		 *		logged in (to default directory)
		 * host:file			dir=NULL, urltype=CLASSIC_URL_T
		 *		"RETR file"
		 * host:dir/			dir="dir", urltype=CLASSIC_URL_T
		 *		"CWD dir", logged in
		 * ftp://host/			dir="", urltype=FTP_URL_T
		 *		logged in (to default directory)
		 * ftp://host/dir/		dir="dir", urltype=FTP_URL_T
		 *		"CWD dir", logged in
		 * ftp://host/file		dir=NULL, urltype=FTP_URL_T
		 *		"RETR file"
		 * ftp://host//file		dir="", urltype=FTP_URL_T
		 *		"CWD ", "RETR file"
		 * host:/file			dir="/", urltype=CLASSIC_URL_T
		 *		"CWD /", "RETR file"
		 * ftp://host///file		dir="/", urltype=FTP_URL_T
		 *		"CWD ", "CWD ", "RETR file"
		 * ftp://host/%2F/file		dir="%2F", urltype=FTP_URL_T
		 *		"CWD /", "RETR file"
		 * ftp://host/foo/file		dir="foo", urltype=FTP_URL_T
		 *		"CWD foo", "RETR file"
		 * ftp://host/foo/bar/file	dir="foo/bar"
		 *		"CWD foo", "CWD bar", "RETR file"
		 * ftp://host//foo/bar/file	dir="/foo/bar"
		 *		"CWD ", "CWD foo", "CWD bar", "RETR file"
		 * ftp://host/foo//bar/file	dir="foo//bar"
		 *		"CWD foo", "CWD ", "CWD bar", "RETR file"
		 * ftp://host/%2F/foo/bar/file	dir="%2F/foo/bar"
		 *		"CWD /", "CWD foo", "CWD bar", "RETR file"
		 * ftp://host/%2Ffoo/bar/file	dir="%2Ffoo/bar"
		 *		"CWD /foo", "CWD bar", "RETR file"
		 * ftp://host/%2Ffoo%2Fbar/file	dir="%2Ffoo%2Fbar"
		 *		"CWD /foo/bar", "RETR file"
		 * ftp://host/%2Ffoo%2Fbar%2Ffile	dir=NULL
		 *		"RETR /foo/bar/file"
		 *
		 * Note that we don't need `dir' after this point.
		 */
		do {
			if (urltype == FTP_URL_T) {
				nextpart = strchr(dir, '/');
				if (nextpart) {
					*nextpart = '\0';
					nextpart++;
				}
				url_decode(dir);
			} else
				nextpart = NULL;
			if (debug)
				fprintf(ttyout, "dir `%s', nextpart `%s'\n",
				    dir ? dir : "<null>",
				    nextpart ? nextpart : "<null>");
			if (urltype == FTP_URL_T || *dir != '\0') {
				xargv[0] = "cd";
				xargv[1] = dir;
				xargv[2] = NULL;
				dirchange = 0;
				cd(2, xargv);
				if (! dirchange) {
					if (*dir == '\0' && code == 500)
						fprintf(stderr,
"\n"
"ftp: The `CWD ' command (without a directory), which is required by\n"
"     RFC 1738 to support the empty directory in the URL pathname (`//'),\n"
"     conflicts with the server's conformance to RFC 959.\n"
"     Try the same URL without the `//' in the URL pathname.\n"
"\n");
					goto cleanup_fetch_ftp;
				}
			}
			dir = nextpart;
		} while (dir != NULL);
	}

	if (EMPTYSTRING(file)) {
		rval = -1;
		goto cleanup_fetch_ftp;
	}

	if (dirhasglob) {
		(void)strlcpy(rempath, dir,	sizeof(rempath));
		(void)strlcat(rempath, "/",	sizeof(rempath));
		(void)strlcat(rempath, file,	sizeof(rempath));
		file = rempath;
	}

			/* Fetch the file(s). */
	xargc = 2;
	xargv[0] = "get";
	xargv[1] = file;
	xargv[2] = NULL;
	if (dirhasglob || filehasglob) {
		int ointeractive;

		ointeractive = interactive;
		interactive = 0;
		xargv[0] = "mget";
		mget(xargc, xargv);
		interactive = ointeractive;
	} else {
		if (outfile == NULL) {
			cp = strrchr(file, '/');	/* find savefile */
			if (cp != NULL)
				outfile = cp + 1;
			else
				outfile = file;
		}
		xargv[2] = (char *)outfile;
		xargv[3] = NULL;
		xargc++;
		if (restartautofetch)
			reget(xargc, xargv);
		else
			get(xargc, xargv);
	}

	if ((code / 100) == COMPLETE)
		rval = 0;

 cleanup_fetch_ftp:
	FREEPTR(host);
	FREEPTR(path);
	FREEPTR(user);
	FREEPTR(pass);
	return (rval);
}

/*
 * Retrieve the given file to outfile.
 * Supports arguments of the form:
 *	"host:path", "ftp://host/path"	if $ftpproxy, call fetch_url() else
 *					call fetch_ftp()
 *	"http://host/path"		call fetch_url() to use HTTP
 *	"file:///path"			call fetch_url() to copy
 *	"about:..."			print a message
 *
 * Returns 1 on failure, 0 on completed xfer, -1 if ftp connection
 * is still open (e.g, ftp xfer with trailing /)
 */
static int
go_fetch(const char *url)
{
	char *proxy;

	/*
	 * Check for about:*
	 */
	if (strncasecmp(url, ABOUT_URL, sizeof(ABOUT_URL) - 1) == 0) {
		url += sizeof(ABOUT_URL) -1;
		if (strcasecmp(url, "ftp") == 0 ||
		    strcasecmp(url, "tnftp") == 0) {
			fputs(
"This version of ftp has been enhanced by Luke Mewburn <lukem@NetBSD.org>\n"
"for the NetBSD project.  Execute `man ftp' for more details.\n", ttyout);
		} else if (strcasecmp(url, "lukem") == 0) {
			fputs(
"Luke Mewburn is the author of most of the enhancements in this ftp client.\n"
"Please email feedback to <lukem@NetBSD.org>.\n", ttyout);
		} else if (strcasecmp(url, "netbsd") == 0) {
			fputs(
"NetBSD is a freely available and redistributable UNIX-like operating system.\n"
"For more information, see http://www.NetBSD.org/\n", ttyout);
		} else if (strcasecmp(url, "version") == 0) {
			fprintf(ttyout, "Version: %s %s%s\n",
			    FTP_PRODUCT, FTP_VERSION,
#ifdef INET6
			    ""
#else
			    " (-IPv6)"
#endif
			);
		} else {
			fprintf(ttyout, "`%s' is an interesting topic.\n", url);
		}
		fputs("\n", ttyout);
		return (0);
	}

	/*
	 * Check for file:// and http:// URLs.
	 */
	if (strncasecmp(url, HTTP_URL, sizeof(HTTP_URL) - 1) == 0 ||
	    strncasecmp(url, FILE_URL, sizeof(FILE_URL) - 1) == 0)
		return (fetch_url(url, NULL, NULL, NULL));

	/*
	 * Try FTP URL-style and host:file arguments next.
	 * If ftpproxy is set with an FTP URL, use fetch_url()
	 * Othewise, use fetch_ftp().
	 */
	proxy = getoptionvalue("ftp_proxy");
	if (!EMPTYSTRING(proxy) &&
	    strncasecmp(url, FTP_URL, sizeof(FTP_URL) - 1) == 0)
		return (fetch_url(url, NULL, NULL, NULL));

	return (fetch_ftp(url));
}

/*
 * Retrieve multiple files from the command line,
 * calling go_fetch() for each file.
 *
 * If an ftp path has a trailing "/", the path will be cd-ed into and
 * the connection remains open, and the function will return -1
 * (to indicate the connection is alive).
 * If an error occurs the return value will be the offset+1 in
 * argv[] of the file that caused a problem (i.e, argv[x]
 * returns x+1)
 * Otherwise, 0 is returned if all files retrieved successfully.
 */
int
auto_fetch(int argc, char *argv[])
{
	volatile int	argpos;
	int		rval;

	argpos = 0;

	if (sigsetjmp(toplevel, 1)) {
		if (connected)
			disconnect(0, NULL);
		return (argpos + 1);
	}
	(void)xsignal(SIGINT, intr);
	(void)xsignal(SIGPIPE, lostpeer);

	/*
	 * Loop through as long as there's files to fetch.
	 */
	for (rval = 0; (rval == 0) && (argpos < argc); argpos++) {
		if (strchr(argv[argpos], ':') == NULL)
			break;
		redirect_loop = 0;
		if (!anonftp)
			anonftp = 2;	/* Handle "automatic" transfers. */
		rval = go_fetch(argv[argpos]);
		if (outfile != NULL && strcmp(outfile, "-") != 0
		    && outfile[0] != '|')
			outfile = NULL;
		if (rval > 0)
			rval = argpos + 1;
	}

	if (connected && rval != -1)
		disconnect(0, NULL);
	return (rval);
}


int
auto_put(int argc, char **argv, const char *uploadserver)
{
	char	*uargv[4], *path, *pathsep;
	int	 uargc, rval, len;

	uargc = 0;
	uargv[uargc++] = "mput";
	uargv[uargc++] = argv[0];
	uargv[2] = uargv[3] = NULL;
	pathsep = NULL;
	rval = 1;

	if (debug)
		fprintf(ttyout, "auto_put: target `%s'\n", uploadserver);

	path = xstrdup(uploadserver);
	len = strlen(path);
	if (path[len - 1] != '/' && path[len - 1] != ':') {
			/*
			 * make sure we always pass a directory to auto_fetch
			 */
		if (argc > 1) {		/* more than one file to upload */
			int len;

			len = strlen(uploadserver) + 2;	/* path + "/" + "\0" */
			free(path);
			path = (char *)xmalloc(len);
			(void)strlcpy(path, uploadserver, len);
			(void)strlcat(path, "/", len);
		} else {		/* single file to upload */
			uargv[0] = "put";
			pathsep = strrchr(path, '/');
			if (pathsep == NULL) {
				pathsep = strrchr(path, ':');
				if (pathsep == NULL) {
					warnx("Invalid URL `%s'", path);
					goto cleanup_auto_put;
				}
				pathsep++;
				uargv[2] = xstrdup(pathsep);
				pathsep[0] = '/';
			} else 
				uargv[2] = xstrdup(pathsep + 1);
			pathsep[1] = '\0';
			uargc++;
		}
	}
	if (debug)
		fprintf(ttyout, "auto_put: URL `%s' argv[2] `%s'\n",
		    path, uargv[2] ? uargv[2] : "<null>");
		
			/* connect and cwd */		 
	rval = auto_fetch(1, &path);
	free(path);
	if(rval >= 0)
		goto cleanup_auto_put;

			/* XXX : is this the best way? */
	if (uargc == 3) {
		uargv[1] = argv[0];
		put(uargc, uargv);
		goto cleanup_auto_put;
	}

	for(; argv[0] != NULL; argv++) {
		uargv[1] = argv[0];	
		mput(uargc, uargv);
	}
	rval = 0;

 cleanup_auto_put:
	FREEPTR(uargv[2]);
	return (rval);
}
