/*-
 * Copyright 1997 Massachusetts Institute of Technology
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby
 * granted, provided that both the above copyright notice and this
 * permission notice appear in all copies, that both the above
 * copyright notice and this permission notice appear in all
 * supporting documentation, and that the name of M.I.T. not be used
 * in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  M.I.T. makes
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied
 * warranty.
 * 
 * THIS SOFTWARE IS PROVIDED BY M.I.T. ``AS IS''.  M.I.T. DISCLAIMS
 * ALL EXPRESS OR IMPLIED WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT
 * SHALL M.I.T. BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$Id: http.c,v 1.14 1997/11/01 05:47:41 ache Exp $
 */

#include <sys/types.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <md5.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <time.h>
#include <unistd.h>

#include <sys/param.h>		/* for MAXHOSTNAMELEN */
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/uio.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include "fetch.h"

struct http_state {
	char *http_hostname;
	char *http_remote_request;
	char *http_decoded_file;
	char *http_host_header;
	char *http_authentication;
	char *http_proxy_authentication;
	unsigned http_port;
	int http_redirected;
};

struct http_auth {
	TAILQ_ENTRY(http_auth) ha_link;
	char *ha_scheme;
	char *ha_realm;
	char *ha_params;
	const struct http_auth_method *ha_ham;
};
TAILQ_HEAD(http_auth_head, http_auth);

static int http_parse(struct fetch_state *fs, const char *uri);
static int http_proxy_parse(struct fetch_state *fs, const char *uri);
static int http_close(struct fetch_state *fs);
static int http_retrieve(struct fetch_state *fs);
static int basic_doauth(struct fetch_state *fs, struct http_auth *ha, int prx);

struct uri_scheme http_scheme =
	{ "http", http_parse, http_proxy_parse, "HTTP_PROXY", "http" };

struct http_auth_head http_auth, http_proxy_auth;

struct http_auth_method {
	const char *ham_scheme;
	int (*ham_doauth)(struct fetch_state *, struct http_auth *, int);
} http_auth_methods[] = {
	{ "basic", basic_doauth },
	{ 0, 0 }
};

/* We are only concerned with headers we might receive. */
enum http_header { 
	ht_accept_ranges, ht_age, ht_allow, ht_cache_control, ht_connection,
	ht_content_base, ht_content_encoding, ht_content_language,
	ht_content_length, ht_content_location, ht_content_md5, 
	ht_content_range, ht_content_type, ht_date, ht_etag, ht_expires,
	ht_last_modified, ht_location, ht_pragma, ht_proxy_authenticate,
	ht_public, ht_retry_after, ht_server, ht_transfer_encoding,
	ht_upgrade, ht_vary, ht_via, ht_www_authenticate, ht_warning,
	/* unusual cases */
	ht_syntax_error, ht_unknown, ht_end_of_header
};

static char *format_http_date(time_t when);
static char *format_http_user_agent(void);
static enum http_header http_parse_header(char *line, char **valuep);
static int check_md5(FILE *fp, char *base64ofmd5);
static int http_first_line(const char *line);
static int http_suck(struct fetch_state *fs, FILE *remote, FILE *local,
		     off_t total_length, int timo);
static int http_suck_chunked(struct fetch_state *fs, FILE *remote, FILE *local,
			     off_t total_length, int timo);
static int parse_http_content_range(char *orig, off_t *first, off_t *total);
static int process_http_auth(struct fetch_state *fs, char *hdr, int autherr);
static struct http_auth *find_http_auth(struct http_auth_head *list,
					const char *scheme, const char *realm);
static time_t parse_http_date(char *datestring);
static void setup_http_auth(void);

static int 
http_parse(struct fetch_state *fs, const char *u)
{
	const char *p, *colon, *slash, *q;
	char *hostname, *hosthdr, *trimmed_name, *uri, *ques, saveq = 0;
	unsigned port;
	struct http_state *https;

	uri = alloca(strlen(u) + 1);
	strcpy(uri, u);

	p = uri + 5;
	port = 0;

	if (p[0] != '/' || p[1] != '/') {
		warnx("`%s': malformed `http' URL", uri);
		return EX_USAGE;
	}

	p += 2;

	if ((ques = strpbrk(p, "?#")) != NULL) {
		saveq = *ques;
		*ques = '\0';
	}

	colon = strchr(p, ':');
	slash = strchr(p, '/');
	if (colon && slash && colon < slash)
		q = colon;
	else
		q = slash;
	if (q == 0) {
		warnx("`%s': malformed `http' URL", uri);
		return EX_USAGE;
	}
	hostname = alloca(q - p + 1);
	hostname[0] = '\0';
	strncat(hostname, p, q - p);
	p = slash;

	if (colon && colon + 1 != slash) {
		unsigned long ul;
		char *ep;

		errno = 0;
		ul = strtoul(colon + 1, &ep, 10);
		if (ep != slash || ep == colon + 1 || errno != 0
		    || ul < 1 || ul > 65534) {
			warn("`%s': invalid port in URL", uri);
			return EX_USAGE;
		}

		port = ul;
	} else {
		port = 80;
	}

	p = slash;

	/* parsing finished, restore parm part */
	if (ques != NULL)
		*ques = saveq;

	https = safe_malloc(sizeof *https);

	/*
	 * Now, we have a copy of the hostname in hostname, the specified port
	 * (or the default value) in port, and p points to the filename part
	 * of the URI.
	 */
	https->http_hostname = safe_strdup(hostname);
	https->http_port = port;
	hosthdr = alloca(sizeof("Host: :\r\n") + 5 + strlen(hostname));
	sprintf(hosthdr, "Host: %s:%d\r\n", hostname, port);
	https->http_host_header = safe_strdup(hosthdr);

	/*
	 * NB: HTTP/1.1 servers MUST also accept a full URI.
	 * However, HTTP/1.0 servers will ONLY accept a trimmed URI.
	 */
	https->http_remote_request = safe_strdup(p);
	p++;
	if (ques) {
		trimmed_name = safe_strndup(p, ques - p);
	} else {
		trimmed_name = safe_strdup(p);
	}
	https->http_decoded_file = percent_decode(trimmed_name);
	free(trimmed_name);
	p = https->http_decoded_file;
	/* now p is the decoded version, so we can extract the basename */

	if (fs->fs_outputfile == 0) {
		slash = strrchr(p, '/');
		if (slash)
			fs->fs_outputfile = slash + 1;
		else
			fs->fs_outputfile = p;
	}
	https->http_redirected = 0;
	https->http_authentication = https->http_proxy_authentication = 0;

	fs->fs_proto = https;
	fs->fs_close = http_close;
	fs->fs_retrieve = http_retrieve;
	return 0;
}

/*
 * An HTTP proxy works by accepting a complete URI in a GET request,
 * retrieving that object, and then forwarding it back to us.  Because
 * it can conceivably handle any URI, we have to do a bit more work
 * in the parsing of it.
 */
static int
http_proxy_parse(struct fetch_state *fs, const char *uri)
{
	struct http_state *https;
	const char *env, *slash, *ques;
	char *file;
	int rv;

	https = safe_malloc(sizeof *https);
	https->http_remote_request = safe_strdup(uri);

	env = getenv("HTTP_PROXY");
	rv = parse_host_port(env, &https->http_hostname, &https->http_port);
	if (rv) {
out:
		free(https->http_remote_request);
		free(https);
		return rv;
	}

	if (strncmp(uri, "http://", 7) == 0 || strncmp(uri, "ftp://", 6) == 0) {
		char *hosthdr;
		slash = strchr(uri + 7, '/');
		if (slash == 0) {
			warnx("`%s': malformed `http' URL", uri);
			rv = EX_USAGE;
			free(https->http_hostname);
			goto out;
		}
		ques = strpbrk(slash, "?#");
		if (ques == 0)
			file = safe_strdup(slash);
		else
			file = safe_strndup(slash, ques - slash);
		hosthdr = alloca(sizeof("Host: \r\n") + slash - uri - 7);
		strcpy(hosthdr, "Host: ");
		strncat(hosthdr, uri + 7, slash - uri - 7);
		strcat(hosthdr, "\r\n");
		https->http_host_header = safe_strdup(hosthdr);
	} else {
		slash = uri;
		while (*slash && *slash != ':')
			slash++;
		if (*slash)
			slash++;
		if (slash[0] == '/' && slash[1] == '/') {
			slash += 2;
			while (*slash && *slash != '/')
				slash++;
		}
		file = safe_strdup(slash);
		https->http_host_header = safe_strdup("");
	}
	https->http_decoded_file = percent_decode(file);
	https->http_redirected = 0;
	https->http_authentication = https->http_proxy_authentication = 0;
	free(file);
	if (fs->fs_outputfile == 0) {
		slash = strrchr(https->http_decoded_file, '/');
		/* NB: we are not guaranteed to find one... */
		fs->fs_outputfile = slash ? slash + 1 
			: https->http_decoded_file;
	}

	fs->fs_proto = https;
	fs->fs_close = http_close;
	fs->fs_retrieve = http_retrieve;
	return 0;
}

static int
http_close(struct fetch_state *fs)
{
	struct http_state *https = fs->fs_proto;

	free(https->http_hostname);
	free(https->http_remote_request);
	free(https->http_decoded_file);
	free(https->http_host_header);
	if (https->http_authentication)
		free(https->http_authentication);
	if (https->http_proxy_authentication)
		free(https->http_proxy_authentication);
	free(https);
	fs->fs_outputfile = 0;
	return 0;
}

static int
nullclose(struct fetch_state *fs)
{
	return 0;
}

/*
 * Process a redirection.  This has a small memory leak.
 */
static int
http_redirect(struct fetch_state *fs, char *new, int permanent)
{
	struct http_state *https = fs->fs_proto;
	int num_redirects = https->http_redirected + 1;
	char *out = safe_strdup(fs->fs_outputfile);
	int rv;

	if (num_redirects > 5) {
		warnx("%s: HTTP redirection limit exceeded");
		return EX_PROTOCOL;
	}

	free(https->http_hostname);
	free(https->http_remote_request);
	free(https->http_decoded_file);
	free(https);
	warnx("%s: resource has moved %s to `%s'", out,
	      permanent ? "permanently" : "temporarily", new);
	rv = http_parse(fs, new);
	if (rv != 0) {
		fs->fs_close = nullclose; /* XXX rethink interface? */
		return rv;
	}
	https = fs->fs_proto;
	https->http_redirected = num_redirects;
	/*
	 * This ensures that the output file name doesn't suddenly change
	 * under the user's feet.  Unfortunately, this results in a small
	 * memory leak.  I wish C had garbage collection...
	 */
	fs->fs_outputfile = out;
	rv = http_retrieve(fs);
	return rv;
}

/*
 * Read HTML-formatted data from remote and display it on stderr.
 * This is extremely incomplete, as all it does is delete anything
 * between angle brackets.  However, this is usually good enough for
 * error messages.
 */
static void
html_display(FILE *remote)
{
	char *line;
	int linelen;
	int inbracket = 0;


	while ((line = fgetln(remote, &linelen)) != 0) {
		char *end = line + linelen;
		char *p;
		int content = 0;

		for (p = line; p < end; p++) {
			if (*p == '<' && !inbracket) {
				fwrite(line, 1, (p - line),
					stderr);
				inbracket = 1;
			}
			if (!inbracket && !content &&
				*p != '\n' && *p != '\r')
				content = 1;
			if (*p == '>' && inbracket) {
				line = p + 1;
				inbracket = 0;
			}
		}
		if (content && line < end)
			fwrite(line, 1, (end - line), stderr);
	}
}

/*
 * Get a file using HTTP.  We will try to implement HTTP/1.1 eventually.
 * This subroutine makes heavy use of the 4.4-Lite standard I/O library,
 * in particular the `fgetln' which allows us to slurp an entire `line'
 * (an arbitrary string of non-NUL characters ending in a newline) directly
 * out of the stdio buffer.  This makes interpreting the HTTP headers much
 * easier, since they are all guaranteed to end in `\r\n' and we can just
 * ignore the `\r'.
 */
static int
http_retrieve(struct fetch_state *fs)
{
	struct http_state *https;
	FILE *remote, *local;
	int s;
	struct sockaddr_in sin;
	struct msghdr msg;
#define NIOV	16	/* max is currently 14 */
	struct iovec iov[NIOV];
	int n, status;
	const char *env;
	int timo;
	char *line, *new_location;
	char *errstr = 0;
	size_t linelen, writeresult;
	off_t total_length, restart_from;
	time_t last_modified, when_to_retry;
	char *base64ofmd5;
	int to_stdout, restarting, redirection, retrying, autherror, chunked;
	char rangebuf[sizeof("Range: bytes=18446744073709551616-\r\n")];

	setup_http_auth();

	https = fs->fs_proto;
	to_stdout = (strcmp(fs->fs_outputfile, "-") == 0);
	restarting = fs->fs_restart;
	redirection = 0;
	retrying = 0;

	/*
	 * Figure out the timeout.  Prefer the -T command-line value,
	 * otherwise the HTTP_TIMEOUT envar, or else don't time out at all.
	 */
	if (fs->fs_timeout) {
		timo = fs->fs_timeout;
	} else if ((env = getenv("HTTP_TIMEOUT")) != 0) {
		char *ep;
		unsigned long ul;

		errno = 0;
		ul = strtoul(env, &ep, 0);
		if (*ep != '\0' || *env == '\0' || errno != 0
		    || ul > INT_MAX) {
			warnx("`%s': invalid timeout", env);
			return EX_USAGE;
		}
		timo = ul;
	} else {
		timo = 0;
	}

	memset(&sin, 0, sizeof sin);
	sin.sin_family = AF_INET;
	sin.sin_len = sizeof sin;
	sin.sin_port = htons(https->http_port);

	fs->fs_status = "looking up hostname";
	if (inet_aton(https->http_hostname, &sin.sin_addr) == 0) {
		struct hostent *hp;

		/* XXX - do timeouts for name resolution? */
		hp = gethostbyname2(https->http_hostname, AF_INET);
		if (hp == 0) {
			warnx("`%s': cannot resolve: %s", https->http_hostname,
			      hstrerror(h_errno));
			return EX_NOHOST;
		}
		memcpy(&sin.sin_addr, hp->h_addr_list[0], sizeof sin.sin_addr);
	}

	fs->fs_status = "creating request message";
	msg.msg_name = (caddr_t)&sin;
	msg.msg_namelen = sizeof sin;
	msg.msg_iov = iov;
	n = 0;
	msg.msg_control = 0;
	msg.msg_controllen = 0;
	msg.msg_flags = fs->fs_linux_bug ? 0 : MSG_EOF;

#define addstr(Iov, N, Str) \
	do { \
		    Iov[N].iov_base = (void *)Str; \
		     Iov[N].iov_len = strlen(Iov[n].iov_base); \
		     N++; \
        } while(0)

retry:
	addstr(iov, n, "GET ");
	addstr(iov, n, https->http_remote_request);
	addstr(iov, n, " HTTP/1.1\r\n");
	/*
	 * The choice of HTTP/1.1 may be a bit controversial.  The 
	 * specification says that implementations which are not at
	 * least conditionally compliant MUST NOT call themselves
	 * HTTP/1.1.  We choose not to comply with that requirement.
	 * (Eventually we will support the full HTTP/1.1, at which
	 * time this comment will not apply.  But it's amusing how
	 * specifications attempt to define behavior for implementations
	 * which aren't obeying the spec in the first place...)
	 */
	addstr(iov, n, format_http_user_agent());
	/* do content negotiation here */
	addstr(iov, n, "Accept: */*\r\n");
	addstr(iov, n, https->http_host_header);
	addstr(iov, n, "Connection: close\r\n");
	if (https->http_proxy_authentication)
		addstr(iov, n, https->http_proxy_authentication);
	if (https->http_authentication)
		addstr(iov, n, https->http_authentication);
	if (fs->fs_mirror) {
		struct stat stab;

		errno = 0;
		if (((!to_stdout && stat(fs->fs_outputfile, &stab) == 0)
		     || (to_stdout && fstat(STDOUT_FILENO, &stab) == 0))
		    && S_ISREG(stab.st_mode)) {
			addstr(iov, n, "If-Modified-Since: ");
			addstr(iov, n, format_http_date(stab.st_mtime));
			addstr(iov, n, "\r\n");
		} else if (errno != 0 || !S_ISREG(stab.st_mode)) {
			if (errno != 0)
				warn("%s", fs->fs_outputfile);
			else
				warnx("%s: not a regular file",
				      fs->fs_outputfile);
			warnx("cannot mirror; will retrieve anew");
		}
	}
	if (restarting) {
		struct stat stab;
		
		errno = 0;
		if (((!to_stdout && stat(fs->fs_outputfile, &stab) == 0)
		     || (to_stdout && fstat(STDOUT_FILENO, &stab) == 0))
		    && S_ISREG(stab.st_mode)) {
			addstr(iov, n, "If-Range: ");
			addstr(iov, n, format_http_date(stab.st_mtime));
			addstr(iov, n, "\r\n");
			sprintf(rangebuf, "Range: bytes=%qd-\r\n", 
				(quad_t)stab.st_size);
			addstr(iov, n, rangebuf);
		} else if (errno != 0 || !S_ISREG(stab.st_mode)) {
			if (errno != 0)
				warn("%s", fs->fs_outputfile);
			else
				warnx("%s: not a regular file",
				      fs->fs_outputfile);
			restarting = 0;
			warnx("cannot restart; will retrieve anew");
		}
	}
	addstr(iov, n, "\r\n");
	msg.msg_iovlen = n;

	if (n >= NIOV)
		err(EX_SOFTWARE, "request vector length exceeded: %d", n);

	s = socket(PF_INET, SOCK_STREAM, 0);
	if (s < 0) {
		warn("socket");
		return EX_OSERR;
	}

	remote = fdopen(s, "r");
	if (remote == 0) {
		warn("fdopen");
		close(s);
		return EX_OSERR;
	}

	fs->fs_status = "sending request message";
	setup_sigalrm();
	alarm(timo);

	/* some hosts do not properly handle T/TCP connections.  If
	 * sendmsg() is used to establish the connection, the OS may
	 * choose to try to use one which could cause the transfer
	 * to fail.  Doing a connect() first ensures that the OS
	 * does not attempt T/TCP.
	 */
	if (fs->fs_use_connect && (connect(s, (struct sockaddr *)&sin, 
                                   sizeof(struct sockaddr_in)) < 0)) {
		warn("connect: %s", https->http_hostname);
		fclose(remote);
		return EX_OSERR;
	}

	if (sendmsg(s, &msg, fs->fs_linux_bug ? 0 : MSG_EOF) < 0) {
		warn("sendmsg: %s", https->http_hostname);
		fclose(remote);
		return EX_OSERR;
	}

got100reply:
	fs->fs_status = "reading reply status";
	alarm(timo);
	line = fgetln(remote, &linelen);
	alarm(0);
	if (line == 0) {
		if (ferror(remote)) {
			warn("reading reply from %s", https->http_hostname);
			fclose(remote);
			unsetup_sigalrm();
			return EX_OSERR;
		} else {
			warnx("empty reply from %s", https->http_hostname);
			fclose(remote);
			unsetup_sigalrm();
			return EX_PROTOCOL;
		}
	}
	/*
	 * If the other end is HTTP 0.9, then we just suck their
	 * response over; can't do anything fancy.  We assume that
	 * the file is a text file, so it is safe to use fgetln()
	 * to suck the entire file.  (It had better be, since
	 * we used it to grab the first line.)
	 */
	if (linelen < 5 || strncasecmp(line, "http", 4) != 0) {
		if (to_stdout)
			local = fopen("/dev/stdout", "w");
		else
			local = fopen(fs->fs_outputfile, "w");
		if (local == 0) {
			warn("%s: fopen", fs->fs_outputfile);
			fclose(remote);
			unsetup_sigalrm();
			return EX_OSERR;
		}
		fs->fs_status = "retrieving from HTTP/0.9 server";
		display(fs, -1, 0);

		do {
			writeresult = fwrite(line, 1, linelen, local);
			display(fs, -1, writeresult);
			if (writeresult != linelen)
				break;
			alarm(timo);
			line = fgetln(remote, &linelen);
			alarm(0);
		} while(line != 0);
		unsetup_sigalrm();

		if (ferror(local)) {
			warn("%s", fs->fs_outputfile);
			fclose(local);
			fclose(remote);
			rm(fs);
			return EX_OSERR;
		} else if(ferror(remote)) {
			warn("%s", https->http_hostname);
			fclose(local);
			fclose(remote);
			rm(fs);
			return EX_OSERR;
		}
		fclose(local);
		fclose(remote);
		display(fs, -1, -1);
		return 0;
	}
	/*
	 * OK.  The other end is doing HTTP 1.0 at the very least.
	 * This means that some of the fancy stuff is at least possible.
	 */
	autherror = 0;
	line[linelen - 1] = '\0'; /* turn line into a string */
	status = http_first_line(line);

	/* In the future, we might handle redirection and other responses. */
	switch(status) {
	case 100:		/* Continue */
		goto got100reply;
	case 200:		/* Here come results */
	case 203:		/* Non-Authoritative Information */
		restarting = 0;
		break;
	case 206:		/* Here come partial results */
		/* can only happen when restarting */
		break;
	case 301:		/* Resource has moved permanently */
		if (!fs->fs_auto_retry)
			errstr = safe_strdup(line);
		else
			redirection = 301;
		break;
	case 302:		/* Resource has moved temporarily */
		/*
		 * We don't test fs->fs_auto_retry here so that this
		 * sort of redirection is transparent to the user.
		 */
		redirection = 302;
		break;
	case 304:		/* Object is unmodified */
		if (fs->fs_mirror) {
			fclose(remote);
			unsetup_sigalrm();
			return 0;
		}
		errstr = safe_strdup(line);
		break;
	case 401:		/* Unauthorized */
		if (https->http_authentication)
			errstr = safe_strdup(line);
		else
			autherror = 401;
		break;
	case 407:		/* Proxy Authentication Required */
		if (https->http_proxy_authentication)
			errstr = safe_strdup(line);
		else
			autherror = 407;
		break;
	case 503:		/* Service Unavailable */
		if (!fs->fs_auto_retry)
			errstr = safe_strdup(line);
		else
			retrying = 503;
		break;

	default:
		errstr = safe_strdup(line);
		break;
	}

	total_length = -1;	/* -1 means ``don't know'' */
	last_modified = when_to_retry = -1;
	base64ofmd5 = 0;
	new_location = 0;
	restart_from = 0;
	chunked = 0;
	fs->fs_status = "parsing reply headers";

	while((line = fgetln(remote, &linelen)) != 0) {
		char *value, *ep;
		enum http_header header;
		unsigned long ul;

		line[linelen - 1] = '\0';
		header = http_parse_header(line, &value);

		if (header == ht_end_of_header)
			break;

		switch(header) {
		case ht_content_length:
			errno = 0;
			ul = strtoul(value, &ep, 10);
			if (errno != 0 || *ep)
				warnx("invalid Content-Length: `%s'", value);
			if (!restarting)
				total_length = ul;
			break;

		case ht_last_modified:
			last_modified = parse_http_date(value);
			if (last_modified == -1 && fs->fs_verbose > 0)
				warnx("invalid Last-Modified: `%s'", value);
			break;
			
		case ht_content_md5:
			base64ofmd5 = safe_strdup(value);
			break;

		case ht_content_range:
			if (!restarting) /* XXX protocol error */
				break;

			/* NB: we might have to restart from farther back
			   than we asked. */
			status = parse_http_content_range(value, &restart_from,
							  &total_length);
			/* If we couldn't understand the reply, get the whole
			   thing. */
			if (status) {
				restarting = 0;
doretry:
				fclose(remote);
				if (base64ofmd5)
					free(base64ofmd5);
				if (new_location)
					free(new_location);
				restart_from = 0;
				n = 0;
				goto retry;
			}
			break;

		case ht_location:
			if (redirection) {
				char *s = value;
				while (*s && !isspace(*s))
					s++;
				new_location = safe_strndup(value, s - value);
			}
			break;

		case ht_transfer_encoding:
			if (strncasecmp(value, "chunked", 7) == 0) {
				chunked = 1;
				break;
			}
			warnx("%s: %s specified Transfer-Encoding `%s'",
			      fs->fs_outputfile, https->http_hostname,
			      value);
			warnx("%s: output file may be uninterpretable",
			      fs->fs_outputfile);
			break;

		case ht_retry_after:
			if (!retrying)
				break;

			errno = 0;
			ul = strtoul(value, &ep, 10);
			if (errno != 0 || (*ep && !isspace(*ep))) {
				time_t when;
				when = parse_http_date(value);
				if (when == -1)
					break;
				when_to_retry = when;
			} else {
				when_to_retry = time(0) + ul;
			}
			break;
				
		case ht_www_authenticate:
			if (autherror != 401)
				break;

			status = process_http_auth(fs, value, autherror);
			if (status != 0)
				goto cantauth;
			break;

		case ht_proxy_authenticate:
			if (autherror != 407)
				break;
			status = process_http_auth(fs, value, autherror);
			if (status != 0)
				goto cantauth;
			break;

		default:
			break;
		}
	}
	if (autherror == 401 && https->http_authentication)
		goto doretry;
	if (autherror == 407 && https->http_proxy_authentication)
		goto doretry;
	if (autherror) {
		goto spewerror;
	}

	if (retrying) {
		int howlong;

		if (when_to_retry == -1) {
			errstr = safe_strdup("HTTP/1.1 503 Service Unavailable");
			goto spewerror;
		}
		howlong = when_to_retry - time(0);
		if (howlong < 30)
			howlong = 30;

		warnx("%s: service unavailable; retrying in %d seconds",
		      https->http_hostname, howlong);
		fs->fs_status = "waiting to retry";
		sleep(howlong);
		goto doretry;
	}

	if (errstr != 0) {
spewerror:
		warnx("%s: %s: HTTP server returned error code %d", 
		      fs->fs_outputfile, https->http_hostname, status);
		if (fs->fs_verbose > 1) {
			fputs(errstr, stderr);
			fputc('\n', stderr);
			html_display(remote);
		}
		free(errstr);
		fclose(remote);
		unsetup_sigalrm();
		return EX_UNAVAILABLE;
	}
		
	if (redirection && new_location) {
		fclose(remote);
		if (base64ofmd5)
			free(base64ofmd5);
		fs->fs_status = "processing redirection";
		status = http_redirect(fs, new_location, redirection == 301);
		free(new_location);
		return status;
	} else if (redirection) {
		warnx("%s: redirection but no new location", 
		      fs->fs_outputfile);
		fclose(remote);
		if (base64ofmd5)
			free(base64ofmd5);
		return EX_PROTOCOL;
	}
		
	fs->fs_status = "retrieving file from HTTP/1.x server";

	/*
	 * OK, if we got here, then we have finished parsing the header
	 * and have read the `\r\n' line which denotes the end of same.
	 * We may or may not have a good idea of the length of the file
	 * or its modtime.  At this point we will have to deal with
	 * any special byte-range, content-negotiation, redirection,
	 * or authentication, and probably jump back up to the top,
	 * once we implement those features.  So, all we have left to
	 * do is open up the output file and copy data from input to
	 * output until EOF.
	 */
	if (to_stdout)
		local = fopen("/dev/stdout", restarting ? "a" : "w");
	else
		local = fopen(fs->fs_outputfile, restarting ? "a" : "w");
	if (local == 0) {
		warn("%s: fopen", fs->fs_outputfile);
		fclose(remote);
		unsetup_sigalrm();
		return EX_OSERR;
	}

	fs->fs_modtime = last_modified;
	fseek(local, restart_from, SEEK_SET); /* XXX truncation off_t->long */
	display(fs, total_length, restart_from); /* XXX truncation */

	if (chunked)
		status = http_suck_chunked(fs, remote, local, total_length, 
					   timo);
	else
		status = http_suck(fs, remote, local, total_length, timo);
	if (status)
		goto out;

	status = errno;		/* save errno for warn(), below, if needed */
	display(fs, total_length, -1); /* do here in case we have to warn */
	errno = status;

	if (ferror(remote)) {
		warn("reading remote file from %s", https->http_hostname);
		status = EX_OSERR;
	} else if(ferror(local)) {
		warn("`%s': fwrite", fs->fs_outputfile);
		status = EX_OSERR;
	} else {
		status = 0;
	}
	if (base64ofmd5) {
		/*
		 * Ack.  When restarting, the MD5 only covers the parts
		 * we are getting, not the whole thing.
		 */
		fseek(local, restart_from, SEEK_SET);
		fs->fs_status = "computing MD5 message digest";
		status = check_md5(local, base64ofmd5);
		free(base64ofmd5);
	}

	fclose(local);
out:
	unsetup_sigalrm();
	fclose(remote);

	if (status != 0)
		rm(fs);
	else
		adjmodtime(fs);

	return status;
#undef addstr

cantauth:
	warnx("%s: cannot authenticate with %s %s",
	      fs->fs_outputfile, 
	      (autherror == 401) ? "server" : "proxy",
	      https->http_hostname);
	status = EX_NOPERM;
	goto out;
}

/*
 * Suck over an HTTP body in standard form.
 */
static int
http_suck(struct fetch_state *fs, FILE *remote, FILE *local, 
	  off_t total_length, int timo)
{
	static char buf[BUFFER_SIZE];
	ssize_t readresult, writeresult;

	do {
		alarm(timo);
		readresult = fread(buf, 1, sizeof buf, remote);
		alarm(0);

		if (readresult == 0)
			return 0;
		display(fs, total_length, readresult);

		writeresult = fwrite(buf, 1, readresult, local);
	} while (writeresult == readresult);
	return 0;
}

/*
 * Suck over an HTTP body in chunked form.  Ick.
 * Note that the return value convention here is a bit strange.
 * A zero return does not necessarily mean success; rather, it means
 * that this routine has already taken care of error reporting and
 * just wants to exit.
 */
static int
http_suck_chunked(struct fetch_state *fs, FILE *remote, FILE *local,
		  off_t total_length, int timo)
{
	static char buf[BUFFER_SIZE];
	ssize_t readresult, writeresult;
	size_t linelen;
	u_long chunklen;
	char *line, *ep;

	for (;;) {
		alarm(timo);
		line = fgetln(remote, &linelen);
		alarm(0);
		if (line == 0) {
			warnx("%s: error processing chunked encoding: "
			      "missing length", fs->fs_outputfile);
			return EX_PROTOCOL;
		}
		line[--linelen] = '\0';
		for (; linelen > 0; linelen--) {
			if (isspace(line[linelen - 1]))
				line[linelen - 1] = '\0';
		}
		errno = 0;
		chunklen = strtoul(line, &ep, 16);
		if (errno || *line == 0
		    || (*ep && !isspace(*ep) && *ep != ';')) {
			warnx("%s: error processing chunked encoding: "
			      "uninterpretable length: %s", line);
			return EX_PROTOCOL;
		}
		if (chunklen == 0)
			break;

#ifndef MIN
#define MIN(a,b) ((a)>(b)?(b):(a))
#endif
		while (chunklen > 0) {
			alarm(timo);
			readresult = fread(buf, 1, MIN(sizeof buf, chunklen),
					   remote);
			alarm(0);
			if (readresult == 0) {
				warnx("%s: EOF with %lu left in chunk",
				      fs->fs_outputfile, chunklen);
				return EX_PROTOCOL;
			}
			display(fs, total_length, readresult);
			chunklen -= readresult;

			writeresult = fwrite(buf, 1, readresult, local);
			if (writeresult != readresult)
				return 0; /* main code will diagnose */
		}
		/*
		 * Read the bogus CRLF after the chunk's body.
		 */
		alarm(timo);
		fread(buf, 1, 2, remote);
		alarm(0);
	}
	/*
	 * If we got here, then we successfully read every chunk and got
	 * the end-of-chunks indicator.  Now we have to ignore any trailer
	 * lines which come across---or we would if we cared about keeping
	 * the connection open.  Since we are just going to close it anyway,
	 * we won't bother with that here.  If ever something important is
	 * defined for the trailer, we will have to revisit that decision.
	 */
	return 0;
}


/*
 * The format of the response line for an HTTP request is:
 *	HTTP/V.vv{WS}999{WS}Explanatory text for humans to read\r\n
 * Old pre-HTTP/1.0 servers can return
 *	HTTP{WS}999{WS}Explanatory text for humans to read\r\n
 * Where {WS} represents whitespace (spaces and/or tabs) and 999
 * is a machine-interprable result code.  We return the integer value
 * of that result code, or the impossible value `0' if we are unable to
 * parse the result.
 */
static int
http_first_line(const char *line)
{
	char *ep;
	unsigned long ul;

	if (strncasecmp(line, "http", 4) != 0)
		return 0;

	line += 4;
	while (*line && !isspace(*line)) /* skip non-whitespace */
		line++;
	while (*line && isspace(*line))	/* skip first whitespace */
		line++;

	errno = 0;
	ul = strtoul(line, &ep, 10);
	if (errno != 0 || ul > 999 || ul < 100 || !isspace(*ep))
		return 0;
	return ul;
}

/*
 * The format of a header line for an HTTP request is:
 *	Header-Name: header-value (with comments in parens)\r\n
 * This would be a nice application for gperf(1), except that the
 * names are case-insensitive and gperf can't handle that.
 */
static enum http_header
http_parse_header(char *line, char **valuep)
{
	char *colon, *value;

	if (*line == '\0' /* protocol error! */
	    || (line[0] == '\r' && line[1] == '\0'))
		return ht_end_of_header;

	colon = strchr(line, ':');
	if (colon == 0)
		return ht_syntax_error;
	*colon = '\0';

	for (value = colon + 1; *value && isspace(*value); value++)
		;		/* do nothing */

	/* Trim trailing whitespace (including \r). */
	*valuep = value;
	value += strlen(value) - 1;
	while (value > *valuep && isspace(*value))
		value--;
	*++value = '\0';

#define cmp(name, num) do { if (!strcasecmp(line, name)) return num; } while(0)
	cmp("Accept-Ranges", ht_accept_ranges);
	cmp("Age", ht_age);
	cmp("Allow", ht_allow);
	cmp("Cache-Control", ht_cache_control);
	cmp("Connection", ht_connection);
	cmp("Content-Base", ht_content_base);
	cmp("Content-Encoding", ht_content_encoding);
	cmp("Content-Language", ht_content_language);
	cmp("Content-Length", ht_content_length);
	cmp("Content-Location", ht_content_location);
	cmp("Content-MD5", ht_content_md5);
	cmp("Content-Range", ht_content_range);
	cmp("Content-Type", ht_content_type);
	cmp("Date", ht_date);
	cmp("ETag", ht_etag);
	cmp("Expires", ht_expires);
	cmp("Last-Modified", ht_last_modified);
	cmp("Location", ht_location);
	cmp("Pragma", ht_pragma);
	cmp("Proxy-Authenticate", ht_proxy_authenticate);
	cmp("Public", ht_public);
	cmp("Retry-After", ht_retry_after);
	cmp("Server", ht_server);
	cmp("Transfer-Encoding", ht_transfer_encoding);
	cmp("Upgrade", ht_upgrade);
	cmp("Vary", ht_vary);
	cmp("Via", ht_via);
	cmp("WWW-Authenticate", ht_www_authenticate);
	cmp("Warning", ht_warning);
#undef cmp
	return ht_unknown;
}

/*
 * Compute the RSA Data Security, Inc., MD5 Message Digest of the file
 * given in `fp', see if it matches the one given in base64 encoding by
 * `base64ofmd5'.  Warn and return an error if it doesn't.
 */
static int
check_md5(FILE *fp, char *base64ofmd5) {
	MD5_CTX ctx;
	unsigned char digest[16];
	char buf[512];
	size_t len;
	char *ourval;

	MD5Init(&ctx);
	while ((len = fread(buf, 1, sizeof buf, fp)) != 0) {
		MD5Update(&ctx, buf, len);
	}
	MD5Final(digest, &ctx);
	ourval = to_base64(digest, 16);
	if (strcmp(ourval, base64ofmd5) != 0) {
		warnx("MD5 digest mismatch: %s, should be %s", ourval,
		      base64ofmd5);
		free(ourval);
		return EX_DATAERR;
	}
	free(ourval);
	return 0;
}

static const char *wkdays[] = { 
	"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};
static const char *months[] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct",
	"Nov", "Dec"
};

/*
 * Interpret one of the three possible formats for an HTTP date.
 * All of them are really bogus; HTTP should use either ISO 8601
 * or NTP timestamps.  We make some attempt to accept a subset of 8601
 * format.  The three standard formats are all fixed-length subsets of their
 * respective standards (except 8601, which puts all of the stuff we
 * care about up front).
 */
static time_t
parse_http_date(char *string)
{
	static struct tm tm;	/* get good initialization */
	time_t rv;
	const char *tz;
	int i;

	/* 8601 has the shortest minimum length */
	if (strlen(string) < 15)
		return -1;

	if (isdigit(*string)) {
		/* ISO 8601: 19970127T134551stuffwedon'tcareabout */
		for (i = 0; i < 15; i++) {
			if (i != 8 && !isdigit(string[i]))
				break;
		}
		if (i < 15)
			return -1;
#define digit(x) (string[x] - '0')
		tm.tm_year = (digit(0) * 1000
			      + digit(1) * 100
			      + digit(2) * 10
			      + digit(3)) - 1900;
		tm.tm_mon = digit(4) * 10 + digit(5) - 1;
		tm.tm_mday = digit(6) * 10 + digit(7);
		if (string[8] != 'T' && string[8] != 't' && string[8] != ' ')
			return -1;
		tm.tm_hour = digit(9) * 10 + digit(10);
		tm.tm_min = digit(11) * 10 + digit(12);
		tm.tm_sec = digit(13) * 10 + digit(14);
		/* We don't care about the rest of the stuff after the secs. */
	} else if (string[3] == ',') {
		/* Mon, 27 Jan 1997 14:24:35 stuffwedon'tcareabout */
		if (strlen(string) < 25)
			return -1;
		string += 5;	/* skip over day-of-week */
		if (!(isdigit(string[0]) && isdigit(string[1])))
			return -1;
		tm.tm_mday = digit(0) * 10 + digit(1);
		for (i = 0; i < 12; i++) {
			if (strncasecmp(months[i], &string[3], 3) == 0)
				break;
		}
		if (i >= 12)
			return -1;
		tm.tm_mon = i;

		if (sscanf(&string[7], "%d %d:%d:%d", &i, &tm.tm_hour,
			   &tm.tm_min, &tm.tm_sec) != 4)
			return -1;
		tm.tm_year = i - 1900;

	} else if (string[3] == ' ') {
		/* Mon Jan 27 14:25:20 1997 */
		if (strlen(string) < 24)
			return -1;
		string += 4;
		for (i = 0; i < 12; i++) {
			if (strncasecmp(string, months[i], 3) == 0)
				break;
		}
		if (i >= 12)
			return -1;
		tm.tm_mon = i;
		if (sscanf(&string[4], "%d %d:%d:%d %u", &tm.tm_mday,
			   &tm.tm_hour, &tm.tm_min, &tm.tm_sec, &i)
		    != 5)
			return -1;
		tm.tm_year = i - 1900;
	} else {
		/* Monday, 27-Jan-97 14:31:09 stuffwedon'tcareabout */
		char *comma = strchr(string, ',');
		char mname[4];

		if (comma == 0)
			return -1;
		string = comma + 1;
		if (strlen(string) < 19)
			return -1;
		string++;
		mname[4] = '\0';
		if (sscanf(string, "%d-%c%c%c-%d %d:%d:%d", &tm.tm_mday,
			   mname, mname + 1, mname + 2, &tm.tm_year,
			   &tm.tm_hour, &tm.tm_min, &tm.tm_sec) != 8)
			return -1;
		for (i = 0; i < 12; i++) {
			if (strcasecmp(months[i], mname))
				break;
		}
		if (i >= 12)
			return -1;
		tm.tm_mon = i;
	}
#undef digit

	if (tm.tm_sec > 60 || tm.tm_min > 59 || tm.tm_hour > 23
	    || tm.tm_mday > 31 || tm.tm_mon > 11)
		return -1;
	if (tm.tm_sec < 0 || tm.tm_min < 0 || tm.tm_hour < 0
	    || tm.tm_mday < 0 || tm.tm_mon < 0 || tm.tm_year < 0)
		return -1;

	tz = getenv("TZ");
	setenv("TZ", "UTC0", 1);
	tzset();
	rv = mktime(&tm);
	if (tz)
		setenv("TZ", tz, 1);
	else
		unsetenv("TZ");
	return rv;
}

static char *
format_http_date(time_t when)
{
	struct tm *tm;
	static char buf[30];

	tm = gmtime(&when);
	if (tm == 0)
		return 0;
#ifndef HTTP_DATE_ISO_8601
	sprintf(buf, "%s, %02d %s %04d %02d:%02d:%02d GMT",
		wkdays[tm->tm_wday], tm->tm_mday, months[tm->tm_mon],
		tm->tm_year + 1900, tm->tm_hour, tm->tm_min, tm->tm_sec);
#else /* ISO 8601 */
	sprintf(buf, "%04d%02d%02dT%02d%02d%02d+0000",
		tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
		tm->tm_hour, tm->tm_min, tm->tm_sec);
#endif
	return buf;
}

static char *
format_http_user_agent(void)
{
	static char buf[128];
	static int inited;

	if (!inited) {
		int mib[2];
		char ostype[128], osrelease[128], machine[128];
		size_t len;

		mib[0] = CTL_KERN;
		mib[1] = KERN_OSTYPE;
		len = sizeof ostype;
		if (sysctl(mib, 2, ostype, &len, 0, 0) < 0) {
			warn("sysctl");
			ostype[0] = '\0';
		}
		mib[1] = KERN_OSRELEASE;
		len = sizeof osrelease;
		if (sysctl(mib, 2, osrelease, &len, 0, 0) < 0) {
			warn("sysctl");
			osrelease[0] = '\0';
		}
		mib[0] = CTL_HW;
		mib[1] = HW_MACHINE;
		len = sizeof machine;
		if (sysctl(mib, 2, machine, &len, 0, 0) < 0) {
			warn("sysctl");
			machine[0] = '\0';
		}

		snprintf(buf, sizeof buf, 
			 "User-Agent: " FETCH_VERSION " %s/%s (%s)\r\n",
			 ostype, osrelease, machine);
	}
	return buf;
}

/*
 * Parse a Content-Range return header from the server.  RFC 2066 defines
 * this header to have the format:
 *	Content-Range: bytes 12345-67890/123456
 * Since we always ask for the whole rest of the file, we consider it an
 * error if the reply doesn't claim to give it to us.
 */
static int
parse_http_content_range(char *orig, off_t *restart_from, off_t *total_length)
{
	u_quad_t first, last, total;
	char *ep;

	if (strncasecmp(orig, "bytes", 5) != 0) {
		warnx("unknown Content-Range unit: `%s'", orig);
		return EX_PROTOCOL;
	}

	orig += 5;
	while (*orig && isspace(*orig))
		orig++;

	errno = 0;
	first = strtouq(orig, &ep, 10);
	if (errno != 0 || *ep != '-') {
		warnx("invalid Content-Range: `%s'", orig);
		return EX_PROTOCOL;
	}
	last = strtouq(ep + 1, &ep, 10);
	if (errno != 0 || *ep != '/' || last < first) {
		warnx("invalid Content-Range: `%s'", orig);
		return EX_PROTOCOL;
	}
	total = strtouq(ep + 1, &ep, 10);
	if (errno != 0 || !(*ep == '\0' || isspace(*ep))) {
		warnx("invalid Content-Range: `%s'", orig);
		return EX_PROTOCOL;
	}

	if (last + 1 != total) {
		warnx("HTTP server did not return requested Content-Range");
		return EX_PROTOCOL;
	}

	*restart_from = first;
	*total_length = last;
	return 0;
}

/*
 * Do HTTP authentication.  We only do ``basic'' right now, but
 * MD5 ought to be fairly easy.  The hard part is actually teasing
 * apart the header, which is fairly badly designed (so what else is
 * new?).
 */
static char *
getauthparam(char *params, const char *name)
{
	char *rv;
	enum state { normal, quoted } state;
	while (*params) {
		if (strncasecmp(params, name, strlen(name)) == 0
		    && params[strlen(name)] == '=')
			break;
		state = normal;
		while (*params) {
			if (state == normal && *params == ',')
				break;
			if (*params == '\"')
				state = (state == quoted) ? normal : quoted;
			if (*params == '\\' && params[1] != '\0')
				params++;
			params++;
		}
	}

	if (*params == '\0')
		return 0;
	params += strlen(name) + 1;
	rv = params;
	state = normal;
	while (*params) {
		if (state == normal && *params == ',')
			break;
		if (*params == '\"')
			state = (state == quoted) ? normal : quoted;
		if (*params == '\\' && params[1] != '\0')
			params++;
		params++;
	}
	if (params[-1] == '\"')
		params[-1] = '\0';
	else
		params[0] = '\0';

	if (*rv == '\"')
		rv++;
	return rv;
}
	
static int
process_http_auth(struct fetch_state *fs, char *hdr, int autherr)
{
	enum state { normal, quoted } state;
	char *scheme, *params, *nscheme, *realm;
	struct http_auth *ha;

	do {
		scheme = params = hdr;
		/* Look for end of scheme name. */
		while (*params && !isspace(*params))
			params++;
		
		if (*params == '\0')
			return EX_PROTOCOL;

		/* Null-terminate scheme and skip whitespace. */
		while (*params && isspace(*params))
			*params++ = '\0';

		/* Semi-parse parameters to find their end. */
		nscheme = params;
		state = normal;
		while (*nscheme) {
			if (state == normal && isspace(*nscheme))
				break;
			if (*nscheme == '\"')
				state = (state == quoted) ? normal : quoted;
			if (*nscheme == '\\' && nscheme[1] != '\0')
				nscheme++;
			nscheme++;
		}

		/* Null-terminate parameters and skip whitespace. */
		while (*nscheme && isspace(*nscheme))
			*nscheme++ = '\0';

		realm = getauthparam(params, "realm");
		if (realm == 0) {
			scheme = nscheme;
			continue;
		}

		if (autherr == 401)
			ha = find_http_auth(&http_auth, scheme, realm);
		else
			ha = find_http_auth(&http_proxy_auth, scheme, realm);

		if (ha)
			return ha->ha_ham->ham_doauth(fs, ha, autherr == 407);
	} while (*scheme);
	return EX_NOPERM;
}

static void
parse_http_auth_env(const char *env, struct http_auth_head *ha_tqh)
{
	char *nenv, *p, *scheme, *realm, *params;
	struct http_auth *ha;
	struct http_auth_method *ham;

	nenv = alloca(strlen(env) + 1);
	strcpy(nenv, env);

	while ((p = strsep(&nenv, " \t")) != 0) {
		scheme = strsep(&p, ":");
		if (scheme == 0 || *scheme == '\0')
			continue;
		realm = strsep(&p, ":");
		if (realm == 0 || *realm == '\0')
			continue;
		params = (p && *p) ? p : 0;
		for (ham = http_auth_methods; ham->ham_scheme; ham++) {
			if (strcasecmp(scheme, ham->ham_scheme) == 0)
				break;
		}
		if (ham == 0)
			continue;
		ha = safe_malloc(sizeof *ha);
		ha->ha_scheme = safe_strdup(scheme);
		ha->ha_realm = safe_strdup(realm);
		ha->ha_params = params ? safe_strdup(params) : 0;
		ha->ha_ham = ham;
		TAILQ_INSERT_TAIL(ha_tqh, ha, ha_link);
	}
}

/*
 * Look up an authentication method.  Automatically clone wildcards
 * into fully-specified entries.
 */
static struct http_auth *
find_http_auth(struct http_auth_head *tqh, const char *scm, const char *realm)
{
	struct http_auth *ha;

	for (ha = tqh->tqh_first; ha; ha = ha->ha_link.tqe_next) {
		if (strcasecmp(ha->ha_scheme, scm) == 0
		    && strcasecmp(ha->ha_realm, realm) == 0)
			return ha;
	}

	for (ha = tqh->tqh_first; ha; ha = ha->ha_link.tqe_next) {
		if (strcasecmp(ha->ha_scheme, scm) == 0
		    && strcmp(ha->ha_realm, "*") == 0)
			break;
	}
	if (ha != 0) {
		struct http_auth *ha2;

		ha2 = safe_malloc(sizeof *ha2);
		ha2->ha_scheme = safe_strdup(scm);
		ha2->ha_realm = safe_strdup(realm);
		ha2->ha_params = ha->ha_params ? safe_strdup(ha->ha_params) :0;
		ha2->ha_ham = ha->ha_ham;
		TAILQ_INSERT_TAIL(tqh, ha2, ha_link);
		ha = ha2;
	}

	return ha;
}
			
static void
setup_http_auth(void)
{
	const char *envar;
	static int once;

	if (once)
		return;
	once = 1;

	TAILQ_INIT(&http_auth);
	TAILQ_INIT(&http_proxy_auth);
	envar = getenv("HTTP_AUTH");
	if (envar)
		parse_http_auth_env(envar, &http_auth);
	
	envar = getenv("HTTP_PROXY_AUTH");
	if (envar)
		parse_http_auth_env(envar, &http_proxy_auth);
}

static int
basic_doauth(struct fetch_state *fs, struct http_auth *ha, int isproxy)
{
	struct http_state *https = fs->fs_proto;
	char *user;
	char *pass;
	char *enc;
	char **hdr;
	size_t userlen;
	FILE *fp;

	if (!isatty(0) && 
	    (ha->ha_params == 0 || strchr(ha->ha_params, ':') == 0))
		return EX_NOPERM;
		
	fp = fopen("/dev/tty", "r+");
	if (fp == 0) {
		warn("opening /dev/tty");
		return EX_OSERR;
	}
	if (ha->ha_params == 0) {
		fprintf(fp, "Enter `basic' user name for realm `%s': ",
			ha->ha_realm);
		fflush(fp);
		user = fgetln(stdin, &userlen);
		if (user == 0 || userlen < 1) { /* longer name? */
			fclose(fp);
			return EX_NOPERM;
		}
		if (user[userlen - 1] == '\n')
			user[userlen - 1] = '\0';
		else
			user[userlen] = '\0';
		user = safe_strdup(user);
		pass = 0;
	} else if ((pass = strchr(ha->ha_params, ':')) == 0) {
		user = safe_strdup(ha->ha_params);
		free(ha->ha_params);
	}

	if (pass == 0) {
		pass = getpass("Password: ");
		ha->ha_params = safe_malloc(strlen(user) + 2 + strlen(pass));
		strcpy(ha->ha_params, user);	
		strcat(ha->ha_params, ":");
		strcat(ha->ha_params, pass);
	}

	enc = to_base64(ha->ha_params, strlen(ha->ha_params));

	hdr = isproxy ? &https->http_proxy_authentication 
		: &https->http_authentication;
	if (*hdr)
		free(*hdr);
	*hdr = safe_malloc(sizeof("Proxy-Authorization: basic \r\n") 
			   + strlen(enc));
	if (isproxy)
		strcpy(*hdr, "Proxy-Authorization");
	else
		strcpy(*hdr, "Authorization");
	strcat(*hdr, ": Basic ");
	strcat(*hdr, enc);
	strcat(*hdr, "\r\n");
	free(enc);
	return 0;
}
