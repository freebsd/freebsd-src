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
 *	$Id$
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
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/uio.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include "fetch.h"

static int http_parse(struct fetch_state *fs, const char *uri);
static int http_proxy_parse(struct fetch_state *fs, const char *uri);
static int http_close(struct fetch_state *fs);
static int http_retrieve(struct fetch_state *fs);

struct uri_scheme http_scheme =
	{ "http", http_parse, http_proxy_parse, "HTTP_PROXY", "http" };

struct http_state {
	char *http_hostname;
	char *http_remote_request;
	char *http_decoded_file;
	unsigned http_port;
};

/* We are only concerned with headers we might receive. */
enum http_header { 
	ht_content_length, ht_last_modified, ht_content_md5, ht_content_type,
	ht_transfer_encoding, ht_content_range, ht_warning,
	/* unusual cases */
	ht_syntax_error, ht_unknown, ht_end_of_header
};

static char *format_http_date(time_t when);
static char *format_http_user_agent(void);
static enum http_header http_parse_header(char *line, char **valuep);
static int check_md5(FILE *fp, char *base64ofmd5);
static int http_first_line(const char *line);
static int parse_http_content_range(char *orig, off_t *first, off_t *total);
static time_t parse_http_date(char *datestring);

static int 
http_parse(struct fetch_state *fs, const char *uri)
{
	const char *p, *colon, *slash, *ques, *q;
	char *hostname;
	unsigned port;
	struct http_state *https;

	p = uri + 5;
	port = 0;

	if (p[0] != '/' || p[1] != '/') {
		warnx("`%s': malformed `http' URL", uri);
		return EX_USAGE;
	}

	p += 2;
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

	p = slash + 1;

	https = malloc(sizeof *https);
	if (https == 0)
		err(EX_OSERR, "malloc");

	/*
	 * Now, we have a copy of the hostname in hostname, the specified port
	 * (or the default value) in port, and p points to the filename part
	 * of the URI.
	 */
	https->http_hostname = safe_strdup(hostname);
	https->http_port = port;

	ques = strpbrk(p, "?#");
	if (ques) {
		https->http_remote_request = safe_strndup(p, ques - p);
	} else {
		https->http_remote_request = safe_strdup(p);
	}
	p = https->http_decoded_file = percent_decode(p);
	/* now p is the decoded version, so we can extract the basename */

	if (fs->fs_outputfile == 0) {
		slash = strrchr(p, '/');
		if (slash)
			fs->fs_outputfile = slash + 1;
		else
			fs->fs_outputfile = p;
	}

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

	https = malloc(sizeof *https);
	https->http_remote_request = safe_strdup(uri);

	env = getenv("HTTP_PROXY");
	rv = parse_host_port(env, &https->http_hostname, &https->http_port);
	if (rv) {
out:
		free(https->http_remote_request);
		free(https);
		return rv;
	}

	if (strncmp(uri, "http://", 7) == 0) {
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
	}
	https->http_decoded_file = percent_decode(file);
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
	free(https);
	fs->fs_outputfile = 0;
	return 0;
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
	struct iovec iov[16];	/* XXX count precisely */
	int n, status;
	const char *env;
	int timo;
	char *line;
	size_t linelen, readresult, writeresult;
	off_t total_length, restart_from;
	time_t last_modified;
	char *base64ofmd5;
	static char buf[BUFFER_SIZE];
	int to_stdout;
	char rangebuf[sizeof("Range: bytes=18446744073709551616-\r\n")];

	https = fs->fs_proto;
	to_stdout = (strcmp(fs->fs_outputfile, "-") == 0);

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

	msg.msg_name = (caddr_t)&sin;
	msg.msg_namelen = sizeof sin;
	msg.msg_iov = iov;
	n = 0;
	msg.msg_control = 0;
	msg.msg_controllen = 0;
	msg.msg_flags = MSG_EOF;

#define addstr(Iov, N, Str) \
	do { \
		    Iov[N].iov_base = (void *)Str; \
		     Iov[N].iov_len = strlen(Iov[n].iov_base); \
		     N++; \
        } while(0)

retry:
	addstr(iov, n, "GET /");
	addstr(iov, n, https->http_remote_request);
	addstr(iov, n, " HTTP/1.0\r\n");
	addstr(iov, n, format_http_user_agent());
	/* do content negotiation here */
	addstr(iov, n, "Accept: */*\r\n");
	if (fs->fs_mirror) {
		struct stat stab;

		errno = 0;
		if (((!to_stdout && stat(fs->fs_outputfile, &stab) == 0)
		     || (to_stdout && fstat(STDOUT_FILENO, &stab) == 0))
		    && S_ISREG(stab.st_mode)) {
			addstr(iov, n, "If-Modified-Since: ");
			addstr(iov, n, format_http_date(stab.st_mtime));
			addstr(iov, n, "\r\n");
		} else if (errno != 0) {
			warn("%s: cannot mirror; will retrieve anew", 
			     fs->fs_outputfile);
		}
	}
	if (fs->fs_restart) {
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
		} else if (errno != 0) {
			warn("%s: cannot restart; will retrieve anew",
			     fs->fs_outputfile);
		}
	}
	addstr(iov, n, "Connection: close\r\n");
	addstr(iov, n, "\r\n");
	msg.msg_iovlen = n;
	
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

	setup_sigalrm();
	alarm(timo);
	if (sendmsg(s, &msg, MSG_EOF) < 0) {
		warn("%s", https->http_hostname);
		fclose(remote);
		return EX_OSERR;
	}

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
	if (linelen < 5 || strncasecmp(line, "http/", 5) != 0) {
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
	line[linelen - 1] = '\0'; /* turn line into a string */
	status = http_first_line(line);

	/* In the future, we might handle redirection and other responses. */
	switch(status) {
	case 200:		/* Here come results */
	case 206:		/* Here come partial results */
		break;

	case 304:		/* Object is unmodified */
		if (fs->fs_mirror) {
			fclose(remote);
			unsetup_sigalrm();
			return 0;
		}
		/* otherwise, fall through */
	default:
		warnx("%s: %s: HTTP server returned error code %d", 
		      fs->fs_outputfile, https->http_hostname, status);
		if (fs->fs_verbose > 1) {
			fputs(line, stderr);
			fputc('\n', stderr);
			while ((line = fgetln(remote, &linelen)) != 0)
				fwrite(line, 1, linelen, stderr);
		}
		fclose(remote);
		unsetup_sigalrm();
		return EX_UNAVAILABLE;
	}

	total_length = -1;	/* -1 means ``don't know'' */
	last_modified = -1;
	base64ofmd5 = 0;
	restart_from = 0;

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
			if (errno != 0 || *ep != '\r')
				warnx("invalid Content-Length: `%s'", value);
			if (!fs->fs_restart)
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
			/* NB: we might have to restart from farther back
			   than we asked. */
			status = parse_http_content_range(value, &restart_from,
							  &total_length);
			/* If we couldn't understand the reply, get the whole
			   thing. */
			if (status) {
				fs->fs_restart = 0;
/*doretry:*/
				fclose(remote);
				if (base64ofmd5)
					free(base64ofmd5);
				restart_from = 0;
				n = 0;
				goto retry;
			}
			break;

		default:
			break;
		}
	}

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
		local = fopen("/dev/stdout", "w");
	else
		local = fopen(fs->fs_outputfile, "w");
	if (local == 0) {
		warn("%s: fopen", fs->fs_outputfile);
		fclose(remote);
		unsetup_sigalrm();
		return EX_OSERR;
	}

	fs->fs_modtime = last_modified;
	fseek(local, restart_from, SEEK_SET); /* XXX truncation off_t->long */
	display(fs, total_length, restart_from); /* XXX truncation */

	do {
		alarm(timo);
		readresult = fread(buf, 1, sizeof buf, remote);
		alarm(0);

		if (readresult == 0)
			break;
		display(fs, total_length, readresult);

		writeresult = fwrite(buf, 1, sizeof buf, local);
	} while (writeresult == readresult);

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
		status = check_md5(local, base64ofmd5);
		free(base64ofmd5);
	}

	unsetup_sigalrm();
	fclose(local);
	fclose(remote);

	if (status != 0)
		rm(fs);
	else
		adjmodtime(fs);

	return status;
#undef addstr
}

/*
 * The format of the response line for an HTTP request is:
 *	HTTP/V.vv{WS}999{WS}Explanatory text for humans to read\r\n
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

	if (strncasecmp(line, "http/", 5) != 0)
		return 0;

	line += 5;
	while (*line && isdigit(*line))	/* skip major version number */
		line++;
	if (*line++ != '.')	/* skip period */
		return 0;
	while (*line && isdigit(*line))	/* skip minor version number */
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

	/* XXX - strip comments? */
	*valuep = value;

#define cmp(name, num) do { if (!strcasecmp(line, name)) return num; } while(0)
	cmp("Content-Length", ht_content_length);
	cmp("Last-Modified", ht_last_modified);
	cmp("Content-MD5", ht_content_md5);
	cmp("Content-Range", ht_content_range);
	cmp("Content-Type", ht_content_type);
	cmp("Transfer-Encoding", ht_transfer_encoding);
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
		if (strlen(string) < 25)
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

	if (strcasecmp(orig, "bytes") != 0) {
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
