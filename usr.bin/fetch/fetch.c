/*-
 * Copyright (c) 2000 Dag-Erling Coïdan Smørgrav
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 *
 *	$FreeBSD$
 */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/socket.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include <fetch.h>

#define MINBUFSIZE	4096

/* Option flags */
int	 A_flag;	/*    -A: do not follow 302 redirects */
int	 a_flag;	/*    -a: auto retry */
size_t	 B_size;	/*    -B: buffer size */
int	 b_flag;	/*!   -b: workaround TCP bug */
char    *c_dirname;	/*    -c: remote directory */
int	 d_flag;	/*    -d: direct connection */
int	 F_flag;	/*    -F: restart without checking mtime  */
char	*f_filename;	/*    -f: file to fetch */
int	 H_flag;	/*    -H: use high port */
char	*h_hostname;	/*    -h: host to fetch from */
int	 l_flag;	/*    -l: link rather than copy file: URLs */
int	 m_flag;	/* -[Mm]: mirror mode */
int	 n_flag;	/*    -n: do not preserve modification time */
int	 o_flag;	/*    -o: specify output file */
int	 o_directory;	/*        output file is a directory */
char	*o_filename;	/*        name of output file */
int	 o_stdout;	/*        output file is stdout */
int	 once_flag;	/*    -1: stop at first successful file */
int	 p_flag;	/* -[Pp]: use passive FTP */
int	 R_flag;	/*    -R: don't delete partially transferred files */
int	 r_flag;	/*    -r: restart previously interrupted transfer */
u_int	 T_secs = 0;	/*    -T: transfer timeout in seconds */
int	 s_flag;        /*    -s: show size, don't fetch */
off_t	 S_size;        /*    -S: require size to match */
int	 t_flag;	/*!   -t: workaround TCP bug */
int	 v_level = 1;	/*    -v: verbosity level */
int	 v_tty;		/*        stdout is a tty */
u_int	 w_secs;	/*    -w: retry delay */
int	 family = PF_UNSPEC;	/* -[46]: address family to use */

int	 sigalrm;	/* SIGALRM received */
int	 sigint;	/* SIGINT received */

u_int	 ftp_timeout;	/* default timeout for FTP transfers */
u_int	 http_timeout;	/* default timeout for HTTP transfers */
u_char	*buf;		/* transfer buffer */


void
sig_handler(int sig)
{
    switch (sig) {
    case SIGALRM:
	sigalrm = 1;
	break;
    case SIGINT:
	sigint = 1;
	break;
    }
}

struct xferstat {
    char		 name[40];
    struct timeval	 start;
    struct timeval	 end;
    struct timeval	 last;
    off_t		 size;
    off_t		 offset;
    off_t		 rcvd;
};

void
stat_display(struct xferstat *xs, int force)
{
    struct timeval now;
    
    if (!v_tty || !v_level)
	return;
    
    gettimeofday(&now, NULL);
    if (!force && now.tv_sec <= xs->last.tv_sec)
	return;
    xs->last = now;
    
    fprintf(stderr, "\rReceiving %s", xs->name);
    if (xs->size == -1)
	fprintf(stderr, ": %lld bytes", xs->rcvd);
    else
	fprintf(stderr, " (%lld bytes): %d%%", xs->size,
		(int)((100.0 * xs->rcvd) / xs->size));
}

void
stat_start(struct xferstat *xs, char *name, off_t size, off_t offset)
{
    snprintf(xs->name, sizeof xs->name, "%s", name);
    gettimeofday(&xs->start, NULL);
    xs->last.tv_sec = xs->last.tv_usec = 0;
    xs->end = xs->last;
    xs->size = size;
    xs->offset = offset;
    xs->rcvd = offset;
    stat_display(xs, 1);
}

void
stat_update(struct xferstat *xs, off_t rcvd, int force)
{
    xs->rcvd = rcvd;
    stat_display(xs, 0);
}

void
stat_end(struct xferstat *xs)
{
    double delta;
    double bps;

    if (!v_level)
	return;
    
    gettimeofday(&xs->end, NULL);
    
    stat_display(xs, 1);
    fputc('\n', stderr);
    delta = (xs->end.tv_sec + (xs->end.tv_usec / 1.e6))
	- (xs->start.tv_sec + (xs->start.tv_usec / 1.e6));
    fprintf(stderr, "%lld bytes transferred in %.1f seconds ",
	    xs->rcvd - xs->offset, delta);
    bps = (xs->rcvd - xs->offset) / delta;
    if (bps > 1024*1024)
	fprintf(stderr, "(%.2f MBps)\n", bps / (1024*1024));
    else if (bps > 1024)
	fprintf(stderr, "(%.2f kBps)\n", bps / 1024);
    else
	fprintf(stderr, "(%.2f Bps)\n", bps);
}

int
fetch(char *URL, char *path)
{
    struct url *url;
    struct url_stat us;
    struct stat sb;
    struct xferstat xs;
    FILE *f, *of;
    size_t size;
    off_t count;
    char flags[8];
    int n, r;
    u_int timeout;

    f = of = NULL;

    /* parse URL */
    if ((url = fetchParseURL(URL)) == NULL) {
	warnx("%s: parse error", URL);
	goto failure;
    }

    timeout = 0;
    *flags = 0;
    count = 0;

    /* common flags */
    if (v_level > 1)
	strcat(flags, "v");
    switch (family) {
    case PF_INET:
	strcat(flags, "4");
	break;
    case PF_INET6:
	strcat(flags, "6");
	break;
    }

    /* FTP specific flags */
    if (strcmp(url->scheme, "ftp") == 0) {
	if (p_flag)
	    strcat(flags, "p");
	if (d_flag)
	    strcat(flags, "d");
	if (H_flag)
	    strcat(flags, "h");
	timeout = T_secs ? T_secs : ftp_timeout;
    }
    
    /* HTTP specific flags */
    if (strcmp(url->scheme, "http") == 0) {
	if (d_flag)
	    strcat(flags, "d");
	if (A_flag)
	    strcat(flags, "A");
	timeout = T_secs ? T_secs : http_timeout;
    }

    /* set the protocol timeout. */
    fetchTimeout = timeout;

    /* just print size */
    if (s_flag) {
	if (fetchStat(url, &us, flags) == -1)
	    goto failure;
	if (us.size == -1)
	    printf("Unknown\n");
	else
	    printf("%lld\n", us.size);
	goto success;
    }

    /*
     * If the -r flag was specified, we have to compare the local and
     * remote files, so we should really do a fetchStat() first, but I
     * know of at least one HTTP server that only sends the content
     * size in response to GET requests, and leaves it out of replies
     * to HEAD requests. Also, in the (frequent) case that the local
     * and remote files match but the local file is truncated, we have
     * sufficient information *before* the compare to issue a correct
     * request. Therefore, we always issue a GET request as if we were
     * sure the local file was a truncated copy of the remote file; we
     * can drop the connection later if we change our minds.
     */
    if (r_flag && !o_stdout && stat(path, &sb) != -1)
	url->offset = sb.st_size;
    else
	sb.st_size = 0;
     
    /* start the transfer */
    if ((f = fetchXGet(url, &us, flags)) == NULL) {
	warnx("%s: %s", path, fetchLastErrString);
	goto failure;
    }
    if (sigint)
	goto signal;
    
    /* check that size is as expected */
    if (S_size) {
	if (us.size == -1) {
	    warnx("%s: size unknown", path);
	    goto failure;
	} else if (us.size != S_size) {
	    warnx("%s: size mismatch: expected %lld, actual %lld",
		  path, S_size, us.size);
	    goto failure;
	}
    }
    
    /* symlink instead of copy */
    if (l_flag && strcmp(url->scheme, "file") == 0 && !o_stdout) {
	if (symlink(url->doc, path) == -1) {
	    warn("%s: symlink()", path);
	    goto failure;
	}
	goto success;
    }

    if (v_level > 1) {
	if (sb.st_size)
	    warnx("local: %lld / %ld", sb.st_size, sb.st_mtime);
	warnx("remote: %lld / %ld", us.size, us.mtime);
    }
    
    /* open output file */
    if (o_stdout) {
	/* output to stdout */
	of = stdout;
    } else if (sb.st_size) {
	/* resume mode, local file exists */
	if (!F_flag && us.mtime && sb.st_mtime != us.mtime) {
	    /* no match! have to refetch */
	    fclose(f);
	    url->offset = 0;
	    if ((f = fetchXGet(url, &us, flags)) == NULL) {
		warnx("%s: %s", path, fetchLastErrString);
		goto failure;
	    }
	    if (sigint)
		goto signal;
	} else {
	    if (us.size == sb.st_size)
		/* nothing to do */
		goto success;
	    if (sb.st_size > us.size) {
		/* local file too long! */
		warnx("%s: local file (%lld bytes) is longer "
		      "than remote file (%lld bytes)",
		      path, sb.st_size, us.size);
		goto failure;
	    }
	    /* we got through, open local file and seek to offset */
	    /*
	     * XXX there's a race condition here - the file we open is not
	     * necessarily the same as the one we stat()'ed earlier...
	     */
	    if ((of = fopen(path, "a")) == NULL) {
		warn("%s: fopen()", path);
		goto failure;
	    }
	    if (fseek(of, url->offset, SEEK_SET) == -1) {
		warn("%s: fseek()", path);
		goto failure;
	    }
	}
    }
    if (m_flag && stat(path, &sb) != -1) {
	/* mirror mode, local file exists */
	if (sb.st_size == us.size && sb.st_mtime == us.mtime)
	    goto success;
    }
    if (!of) {
	/*
	 * We don't yet have an output file; either this is a vanilla
	 * run with no special flags, or the local and remote files
	 * didn't match.
	 */
	if ((of = fopen(path, "w")) == NULL) {
	    warn("%s: open()", path);
	    goto failure;
	}
    }
    count = url->offset;

    /* start the counter */
    stat_start(&xs, path, us.size, count);

    sigint = sigalrm = 0;

    /* suck in the data */
    for (n = 0; !sigint && !sigalrm; ++n) {
	if (us.size != -1 && us.size - count < B_size)
	    size = us.size - count;
	else
	    size = B_size;
	if (timeout)
	    alarm(timeout);
	if ((size = fread(buf, 1, size, f)) <= 0)
	    break;
	stat_update(&xs, count += size, 0);
	if (fwrite(buf, size, 1, of) != 1)
	    break;
    }

    if (timeout)
	alarm(0);

    stat_end(&xs);

    /* Set mtime of local file */
    if (!n_flag && us.mtime && !o_stdout
	&& (stat(path, &sb) != -1) && sb.st_mode & S_IFREG) {
	struct timeval tv[2];
	
	fflush(of);
	tv[0].tv_sec = (long)(us.atime ? us.atime : us.mtime);
	tv[1].tv_sec = (long)us.mtime;
	tv[0].tv_usec = tv[1].tv_usec = 0;
	if (utimes(path, tv))
	    warn("%s: utimes()", path);
    }
    
    /* timed out or interrupted? */
 signal:
    if (sigalrm)
	warnx("transfer timed out");
    if (sigint) {
	warnx("transfer interrupted");
	goto failure;
    }
    
    if (!sigalrm) {
	/* check the status of our files */
	if (ferror(f))
	    warn("%s", URL);
	if (ferror(of))
	    warn("%s", path);
	if (ferror(f) || ferror(of))
	    goto failure;
    }

    /* did the transfer complete normally? */
    if (us.size != -1 && count < us.size) {
	warnx("%s appears to be truncated: %lld/%lld bytes",
	      path, count, us.size);
	goto failure_keep;
    }
    
 success:
    r = 0;
    goto done;
 failure:
    if (of && of != stdout && !R_flag && !r_flag)
	if (stat(path, &sb) != -1 && (sb.st_mode & S_IFREG))
	    unlink(path);
 failure_keep:
    r = -1;
    goto done;
 done:
    if (f)
	fclose(f);
    if (of && of != stdout)
	fclose(of);
    if (url)
	fetchFreeURL(url);
    return r;
}

void
usage(void)
{
    /* XXX badly out of synch */
    fprintf(stderr,
	    "Usage: fetch [-1AFHMPRabdlmnpqrstv] [-o outputfile] [-S bytes]\n"
	    "             [-B bytes] [-T seconds] [-w seconds]\n"
	    "             [-f file -h host [-c dir] | URL ...]\n"
	);
}


#define PARSENUM(NAME, TYPE)		\
int					\
NAME(char *s, TYPE *v)			\
{					\
    *v = 0;				\
    for (*v = 0; *s; s++)		\
	if (isdigit(*s))		\
	    *v = *v * 10 + *s - '0';	\
	else				\
	    return -1;			\
    return 0;				\
}

PARSENUM(parseint, u_int)
PARSENUM(parsesize, size_t)
PARSENUM(parseoff, off_t)

int
main(int argc, char *argv[])
{
    struct stat sb;
    struct sigaction sa;
    char *p, *q, *s;
    int c, e, r;

    while ((c = getopt(argc, argv,
		       "146AaB:bc:dFf:h:lHMmnPpo:qRrS:sT:tvw:")) != EOF)
	switch (c) {
	case '1':
	    once_flag = 1;
	    break;
	case '4':
	    family = PF_INET;
	    break;
	case '6':
	    family = PF_INET6;
	    break;
	case 'A':
	    A_flag = 1;
	    break;
	case 'a':
	    a_flag = 1;
	    break;
	case 'B':
	    if (parsesize(optarg, &B_size) == -1)
		errx(1, "invalid buffer size");
	    break;
	case 'b':
	    warnx("warning: the -b option is deprecated");
	    b_flag = 1;
	    break;
	case 'c':
	    c_dirname = optarg;
	    break;
	case 'd':
	    d_flag = 1;
	    break;
	case 'F':
	    F_flag = 1;
	    break;
	case 'f':
	    f_filename = optarg;
	    break;
	case 'H':
	    H_flag = 1;
	    break;
	case 'h':
	    h_hostname = optarg;
	    break;
	case 'l':
	    l_flag = 1;
	    break;
	case 'o':
	    o_flag = 1;
	    o_filename = optarg;
	    break;
	case 'M':
	case 'm':
	    if (r_flag)
		errx(1, "the -m and -r flags are mutually exclusive");
	    m_flag = 1;
	    break;
	case 'n':
	    n_flag = 1;
	    break;
	case 'P':
	case 'p':
	    p_flag = 1;
	    break;
	case 'q':
	    v_level = 0;
	    break;
	case 'R':
	    R_flag = 1;
	    break;
	case 'r':
	    if (m_flag)
		errx(1, "the -m and -r flags are mutually exclusive");
	    r_flag = 1;
	    break;
	case 'S':
	    if (parseoff(optarg, &S_size) == -1)
		errx(1, "invalid size");
	    break;
	case 's':
	    s_flag = 1;
	    break;
	case 'T':
	    if (parseint(optarg, &T_secs) == -1)
		errx(1, "invalid timeout");
	    break;
	case 't':
	    t_flag = 1;
	    warnx("warning: the -t option is deprecated");
	    break;
	case 'v':
	    v_level++;
	    break;
	case 'w':
	    a_flag = 1;
	    if (parseint(optarg, &w_secs) == -1)
		errx(1, "invalid delay");
	    break;
	default:
	    usage();
	    exit(EX_USAGE);
	}

    argc -= optind;
    argv += optind;

    if (h_hostname || f_filename || c_dirname) {
	if (!h_hostname || !f_filename || argc) {
	    usage();
	    exit(EX_USAGE);
	}
	/* XXX this is a hack. */
	if (strcspn(h_hostname, "@:/") != strlen(h_hostname))
	    errx(1, "invalid hostname");
	if (asprintf(argv, "ftp://%s/%s/%s", h_hostname,
		     c_dirname ? c_dirname : "", f_filename) == -1)
	    errx(1, strerror(ENOMEM));
	argc++;
    }

    if (!argc) {
	usage();
	exit(EX_USAGE);
    }

    /* allocate buffer */
    if (B_size < MINBUFSIZE)
	B_size = MINBUFSIZE;
    if ((buf = malloc(B_size)) == NULL)
	errx(1, strerror(ENOMEM));

    /* timeouts */
    if ((s = getenv("FTP_TIMEOUT")) != NULL) {
	if (parseint(s, &ftp_timeout) == -1) {
	    warnx("FTP_TIMEOUT is not a positive integer");
	    ftp_timeout = 0;
	}
    }
    if ((s = getenv("HTTP_TIMEOUT")) != NULL) {
	if (parseint(s, &http_timeout) == -1) {
	    warnx("HTTP_TIMEOUT is not a positive integer");
	    http_timeout = 0;
	}
    }

    /* signal handling */
    sa.sa_flags = 0;
    sa.sa_handler = sig_handler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGALRM, &sa, NULL);
    sa.sa_flags = SA_RESETHAND;
    sigaction(SIGINT, &sa, NULL);
    fetchRestartCalls = 0;
    
    /* output file */
    if (o_flag) {
	if (strcmp(o_filename, "-") == 0) {
	    o_stdout = 1;
	} else if (stat(o_filename, &sb) == -1) {
	    if (errno == ENOENT) {
		if (argc > 1)
		    errx(EX_USAGE, "%s is not a directory", o_filename);
	    } else {
		err(EX_IOERR, "%s", o_filename);
	    }
	} else {
	    if (sb.st_mode & S_IFDIR)
		o_directory = 1;
	}
    }

    /* check if output is to a tty (for progress report) */
    v_tty = isatty(STDERR_FILENO);
    r = 0;

    while (argc) {
	if ((p = strrchr(*argv, '/')) == NULL)
	    p = *argv;
	else
	    p++;

	if (!*p)
	    p = "fetch.out";
	
	fetchLastErrCode = 0;
	
	if (o_flag) {
	    if (o_stdout) {
		e = fetch(*argv, "-");
	    } else if (o_directory) {
		asprintf(&q, "%s/%s", o_filename, p);
		e = fetch(*argv, q);
		free(q);
	    } else {
		e = fetch(*argv, o_filename);
	    }
	} else {
	    e = fetch(*argv, p);
	}

	if (sigint)
	    kill(getpid(), SIGINT);
	
	if (e == 0 && once_flag)
	    exit(0);
	
	if (e) {
	    r = 1;
	    if ((fetchLastErrCode
		 && fetchLastErrCode != FETCH_UNAVAIL
		 && fetchLastErrCode != FETCH_MOVED
		 && fetchLastErrCode != FETCH_URL
		 && fetchLastErrCode != FETCH_RESOLV
		 && fetchLastErrCode != FETCH_UNKNOWN)) {
		if (w_secs) {
		    if (v_level)
			fprintf(stderr, "Waiting %d seconds before retrying\n",
				w_secs);
		    sleep(w_secs);
		}
		if (a_flag)
		    continue;
	    }
	}

	argc--, argv++;
    }
    
    exit(r);
}
