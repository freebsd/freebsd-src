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
int	 d_flag;	/*    -d: direct connection */
int	 F_flag;	/*    -F: restart without checking mtime  */
char	*f_filename;	/*    -f: file to fetch */
int	 H_flag;	/*    -H: use high port */
char	*h_hostname;	/*    -h: host to fetch from */
int	 l_flag;	/*    -l: link rather than copy file: URLs */
int	 m_flag;	/* -[Mm]: set local timestamp to remote timestamp */
int	 o_flag;	/*    -o: specify output file */
int	 o_directory;	/*        output file is a directory */
char	*o_filename;	/*        name of output file */
int	 o_stdout;	/*        output file is stdout */
int	 once_flag;	/*    -1: stop at first successful file */
int	 p_flag = 1;	/* -[Pp]: use passive FTP */
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


u_int	 ftp_timeout;	/* default timeout for FTP transfers */
u_int	 http_timeout;	/* default timeout for HTTP transfers */
u_char	*buf;		/* transfer buffer */


void
sig_handler(int sig)
{
    errx(1, "Transfer timed out");
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
stat_start(struct xferstat *xs, char *name, off_t size, off_t offset)
{
    snprintf(xs->name, sizeof xs->name, "%s", name);
    xs->size = size;
    xs->offset = offset;
    if (v_level) {
	fprintf(stderr, "Receiving %s", xs->name);
	if (xs->size != -1)
	    fprintf(stderr, " (%lld bytes)", xs->size - xs->offset);
    }
    gettimeofday(&xs->start, NULL);
    xs->last = xs->start;
}

void
stat_update(struct xferstat *xs, off_t rcvd)
{
    struct timeval now;
    
    xs->rcvd = rcvd;

    if (v_level <= 1 || !v_tty)
	return;
    
    gettimeofday(&now, NULL);
    if (now.tv_sec <= xs->last.tv_sec)
	return;
    xs->last = now;
    
    fprintf(stderr, "\rReceiving %s", xs->name);
    if (xs->size == -1)
	fprintf(stderr, ": %lld bytes", xs->rcvd - xs->offset);
    else
	fprintf(stderr, " (%lld bytes): %d%%", xs->size - xs->offset,
		(int)((100.0 * xs->rcvd) / (xs->size - xs->offset)));
}

void
stat_end(struct xferstat *xs)
{
    double delta;
    double bps;
    
    gettimeofday(&xs->end, NULL);
    
    if (!v_level)
	return;
    
    fputc('\n', stderr);
    delta = (xs->end.tv_sec + (xs->end.tv_usec / 1.e6))
	- (xs->start.tv_sec + (xs->start.tv_usec / 1.e6));
    fprintf(stderr, "%lld bytes transferred in %.1f seconds ",
	    xs->size - xs->offset, delta);
    bps = (xs->size - xs->offset) / delta;
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
    int ch, n, r;
    u_int timeout;

    f = of = NULL;

    /* parse URL */
    if ((url = fetchParseURL(URL)) == NULL) {
	warnx("%s: parse error", URL);
	goto failure;
    }

    timeout = 0;
    *flags = 0;

    /* common flags */
    if (v_level > 2)
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

    /*
     * Set the protocol timeout.
     * This currently only works for FTP, so we still use
     * alarm(timeout) further down.
     */
    fetchTimeout = timeout;

    /* stat remote file */
    alarm(timeout);
    if (fetchStat(url, &us, flags) == -1)
	warnx("%s: size not known", path);
    alarm(timeout);

    /* just print size */
    if (s_flag) {
	if (us.size == -1)
	    printf("Unknown\n");
	else
	    printf("%lld\n", us.size);
	goto success;
    }

    /* check that size is as expected */
    if (S_size && us.size != -1 && us.size != S_size) {
	warnx("%s: size mismatch: expected %lld, actual %lld",
	      path, S_size, us.size);
	goto failure;
    }
    
    /* symlink instead of copy */
    if (l_flag && strcmp(url->scheme, "file") == 0 && !o_stdout) {
	if (symlink(url->doc, path) == -1) {
	    warn("%s: symlink()", path);
	    goto failure;
	}
	goto success;
    }

    if (o_stdout) {
	/* output to stdout */
	of = stdout;
    } else if (r_flag && us.size != -1 && stat(path, &sb) != -1
	       && (F_flag || (us.mtime && sb.st_mtime == us.mtime))) {
	/* output to file, restart aborted transfer */
	if (us.size == sb.st_size)
	    goto success;
	else if (sb.st_size > us.size && truncate(path, us.size) == -1) {
	    warn("%s: truncate()", path);
	    goto failure;
	}
	if ((of = fopen(path, "a")) == NULL) {
	    warn("%s: open()", path);
	    goto failure;
	}
	url->offset = sb.st_size;
    } else if (m_flag && us.size != -1 && stat(path, &sb) != -1) {
	/* output to file, mirror mode */
	warnx(" local: %lld bytes, mtime %ld", sb.st_size, sb.st_mtime);
	warnx("remote: %lld bytes, mtime %ld", us.size, us.mtime);
	if (sb.st_size == us.size && sb.st_mtime == us.mtime)
	    return 0;
	if ((of = fopen(path, "w")) == NULL) {
	    warn("%s: open()", path);
	    goto failure;
	}
    } else {
	/* output to file, all other cases */
	if ((of = fopen(path, "w")) == NULL) {
	    warn("%s: open()", path);
	    goto failure;
	}
    }
    count = url->offset;

    /* start the transfer */
    if ((f = fetchGet(url, flags)) == NULL) {
	warnx("%s", fetchLastErrString);
	goto failure;
    }
    
    /* start the counter */
    stat_start(&xs, path, us.size, count);

    n = 0;

    if (us.size == -1) {
	/*	  
	 * We have no idea how much data to expect, so do it byte by
         * byte. This is incredibly inefficient, but there's not much
         * we can do about it... :(	 
	 */
	while (1) {
	    if (timeout)
		alarm(timeout);
#ifdef STDIO_HACK
	    /*	      
	     * This is a non-portable hack, but it makes things go
	     * faster. Basically, if there is data in the input file's
	     * buffer, write it out; then fall through to the fgetc()
	     * which forces a refill. It saves a memcpy() and reduces
	     * the number of iterations, i.e the number of calls to
	     * alarm(). Empirical evidence shows this can cut user
	     * time by up to 90%. There may be better (even portable)
	     * ways to do this.
	     */
	    if (f->_r && (f->_ub._base == NULL)) {
		if (fwrite(f->_p, f->_r, 1, of) < 1)
		    break;
		count += f->_r;
		f->_p += f->_r;
		f->_r = 0;
	    }
#endif
	    if ((ch = fgetc(f)) == EOF || fputc(ch, of) == EOF)
		break;
	    stat_update(&xs, count++);
	    n++;
	}
    } else {
	/* we know exactly how much to transfer, so do it efficiently */
	for (size = B_size; count != us.size; n++) {
	    if (us.size - count < B_size)
		size = us.size - count;
	    if (timeout)
		alarm(timeout);
	    if (fread(buf, size, 1, f) != 1 || fwrite(buf, size, 1, of) != 1)
		break;
	    stat_update(&xs, count += size);
	}
    }
    
    if (timeout)
	alarm(0);

    stat_end(&xs);
    
    /* check the status of our files */
    if (ferror(f))
	warn("%s", URL);
    if (ferror(of))
	warn("%s", path);
    if (ferror(f) || ferror(of)) {
	if (!R_flag && !o_stdout)
	    unlink(path);
	goto failure;
    }

    /* need to close the file before setting mtime */
    if (of != stdout) {
	fclose(of);
	of = NULL;
    }
    
    /* Set mtime of local file */
    if (m_flag && us.size != -1 && !o_stdout) {
	struct timeval tv[2];
	
	tv[0].tv_sec = (long)us.atime;
	tv[1].tv_sec = (long)us.mtime;
	tv[0].tv_usec = tv[1].tv_usec = 0;
	if (utimes(path, tv))
	    warn("%s: utimes()", path);
    }
    
 success:
    r = 0;
    goto done;
 failure:
    r = -1;
    goto done;
 done:
    if (f)
	fclose(f);
    if (of && of != stdout)
	fclose(of);
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
    char *p, *q, *s;
    int c, e, r;

    while ((c = getopt(argc, argv,
		       "146AaB:bdFf:h:lHMmnPpo:qRrS:sT:tvw:")) != EOF)
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
	    m_flag = 1;
	    break;
	case 'n':
	    m_flag = 0;
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

    if (h_hostname || f_filename) {
	if (!h_hostname || !f_filename || argc) {
	    usage();
	    exit(EX_USAGE);
	}
	/* XXX this is a hack. */
	if (strcspn(h_hostname, "@:/") != strlen(h_hostname))
	    errx(1, "invalid hostname");
	if (asprintf(argv, "ftp://%s/%s", h_hostname, f_filename) == -1)
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

    /* timeout handling */
    signal(SIGALRM, sig_handler);
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
    v_tty = isatty(STDOUT_FILENO);
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
			fprintf(stderr, "Waiting %d seconds before retrying\n", w_secs);
		    sleep(w_secs);
		}
		if (a_flag)
		    continue;
		fprintf(stderr, "Skipping %s\n", *argv);
	    }
	}

	argc--, argv++;
    }
    
    exit(r);
}
