/*-
 * Copyright (c) 1996
 *      Jean-Marc Zucconi
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 *  OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* $Id: main.c,v 1.24 1996/10/06 00:44:24 jmz Exp $ */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pwd.h>
#include <unistd.h>

#include <netinet/in.h>

#include <arpa/inet.h>

#include <err.h>
#include <errno.h>
#include <netdb.h>
#include <pwd.h>
#include <regex.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <ftpio.h>

#define BUFFER_SIZE 1024
#define HTTP_TIMEOUT 300 /* seconds */
#define FTP_TIMEOUT 300 /* seconds */

char buffer[BUFFER_SIZE];

extern char *__progname;		/* from crt0.o */

int verbose = 1;
int ftp_verbose = 0;
int linkfile = 0;
char *outputfile = 0;
char *change_to_dir = 0;
char *host = 0;
int passive_mode = 0;
char *file_to_get = 0;
int ftp = 0;
int http_proxy = 0;
int http = 0;
int http_port = 80;
int mirror = 0;
int newtime = 0;
int restart = 0;
time_t modtime;
FILE *file = 0;
int timeout_ival = 0;

void usage(void), die(int), rm(void), timeout(int), ftpget(void),
    httpget(void), fileget(void),
    display(int, int), parse(char *), output_file_name(void),
    f_size(char *, off_t *, time_t *), ftperr(FILE* ftp, char *, ...),
    filter(unsigned char *, int),
    setup_http_proxy(void);

int match(char *, char *), http_open(void);

void
usage()
{
    fprintf(stderr, "usage: %s [-DHINPMTVLqlmnprv] [-o outputfile] <-f file -h host [-c dir]| URL>\n", __progname);
    exit(1);
}

void
die(int sig)
{
    int e = errno;
    
    rm();
    if (!sig)
	fprintf (stderr, "%s: %s\n", __progname, strerror(e));
    else
	warnx ("Interrupted by signal %d", sig);
    exit(1);
}

void
adjmodtime()
{
    struct timeval tv[2];
    
    if (!newtime) {
	tv[0].tv_usec = tv[1].tv_usec = 0;
	tv[0].tv_sec = time(0);
	tv[1].tv_sec = modtime;
	utimes (outputfile, tv);
    }
}

void
rm()
{
    if (file) {
	fclose(file);
	if (file != stdout) {
	    if (!restart && !mirror)
		remove(outputfile);
	    adjmodtime();
	}
    }
}

int
main(int argc, char **argv)
{
    int c;

    while ((c = getopt (argc, argv, "D:HINPMT:V:Lqc:f:h:o:plmnrv")) != -1) {
	switch (c) {
	case 'D': case 'H': case 'I': case 'N': case 'L': case 'V': 
	    break;	/* ncftp compatibility */
	    
	case 'q': 
	    verbose = 0;
	    
	case 'c':
	    change_to_dir = optarg;
	    break;
	    
	case 'f':
	    file_to_get = optarg;
	    break;
	    
	case 'h':
	    host = optarg;
	    ftp = 1;
	    break;

	case 'l':
	    linkfile = 1;
	    break;

	case 'o':
	    outputfile = optarg;
	    break;
	    
	case 'p': case 'P':
	    passive_mode = 1;
	    break;
	    
	case 'm': case 'M':
	    mirror = 1;
	    break;
	    
	case 'n':
	    newtime = 1;
	    break;
	    
	case 'r':
	    restart = 1;
	    break;
	    
	case 'v':
	    ftp_verbose = 1;
	    break;

	case 'T':
	    timeout_ival = atoi(optarg);
	    break;

	default: 	
	case '?':
	    usage();
	}
    }
    argc -= optind;
    argv += optind;
    if (argv[0]) {
	if (host || change_to_dir || file_to_get)
	    usage();
	parse(argv[0]);
    } else {
	if (!host || !file_to_get)
	    usage();
    }
    
    if (mirror && restart)
	errx(1, "-m and -r are mutually exclusive.");
    
    output_file_name();
    
    signal(SIGHUP, die);
    signal(SIGINT, die);
    signal(SIGQUIT, die);
    signal(SIGTERM, die);
    
    setup_http_proxy();

    if (http)
	httpget();
    else if (ftp)
	ftpget();
    else
	fileget();
    exit(0);
}

void
timeout(int sig)
{
    fprintf (stderr, "\n%s: Timeout\n", __progname);
    rm();
    exit(1);
}

void
fileget()
{
    char *basename;

    if (access(file_to_get, R_OK)) {
	fprintf(stderr, "unable to access local file `%s'\n", file_to_get);
	exit(1);
    }
    basename = strrchr(file_to_get, '/');
    if (!basename) {
	fprintf(stderr, "malformed filename `%s' - expected full path.\n",
		file_to_get);
	exit(1);
    }
    ++basename;	/* move over the / */
    if (!access(basename, F_OK)) {
	fprintf(stderr, "%s: file already exists.\n", basename);
	exit(1);
    }
    if (linkfile) {
	if (symlink(file_to_get, basename) == -1) {
	    perror("symlink");
	    exit(1);
	}
    }
    else {
	char *buf = alloca(strlen(file_to_get) + strlen(basename) + 15);

	sprintf(buf, "/bin/cp -p %s %s", file_to_get, basename);
	if (system(buf)) {
	    fprintf(stderr, "failed to copy %s successfully.", file_to_get);
	    exit(1);
	}
    }
}

void
ftpget()
{
    FILE *ftp, *fp;
    char *cp, *lp;
    int status, n;
    off_t size, size0, seekloc;
    char ftp_pw[200];
    time_t t;
    time_t tout;
    struct itimerval timer;
    
    if ((cp = getenv("FTP_PASSWORD")) != NULL)
	strcpy(ftp_pw, cp);
    else {
	sprintf (ftp_pw, "%s@", getpwuid (getuid ())->pw_name);
	n = strlen (ftp_pw);
	gethostname (ftp_pw + n, 200 - n);
    }
    if ((lp = getenv("FTP_LOGIN")) == NULL)
	lp = "anonymous";
    ftp = ftpLogin(host, lp, ftp_pw, 0, ftp_verbose);
    if (!ftp) 
	err(1, "couldn't open FTP connection or login to %s.", host);
    
    /* Time to set our defaults */
    ftpBinary (ftp);
    ftpPassive (ftp, passive_mode);
    
    if (change_to_dir) {
	status = ftpChdir (ftp, change_to_dir);
	if (status)
	    ftperr (ftp, "couldn't cd to %s: ", change_to_dir);
    }
    size = ftpGetSize (ftp, file_to_get);
    modtime = ftpGetModtime (ftp, file_to_get);
    if (modtime < -1) {
	warnx ("Couldn't get file time for %s - using current time", file_to_get);
	modtime = (time_t) -1;
    }
    
    if (!strcmp (outputfile, "-"))
	restart = 0;
    if (restart || mirror) {
	f_size (outputfile, &size0, &t);
	if (mirror && size0 == size && modtime <= t) {
	    fclose(ftp);
	    return;
	}
	else if (restart) {
	    if (size0 && size0 < size)
		seekloc = size0;
	    else
		seekloc = size0 = 0;
	}
    }	    
    else if (!restart)
	seekloc = size0 = 0;
    
    fp = ftpGet (ftp, file_to_get, &seekloc);
    if (fp == NULL)
	if (ftpErrno(ftp))
	    ftperr (ftp, NULL);
	else
	    die(0);
    if (size0 && !seekloc)
	size0 = 0;
    
    if (strcmp (outputfile, "-")) {
	file = fopen (outputfile, size0 ? "a" : "w");
	if (!file) 
	    err (1, "could not open output file %s.", outputfile);
    } else 
	file = stdout;
    
    signal (SIGALRM, timeout);
    if (timeout_ival)
	tout = timeout_ival;
    else if ((cp = getenv("FTP_TIMEOUT")) != NULL)
	tout = atoi(cp);
    else
	tout = FTP_TIMEOUT;

    
    timer.it_interval.tv_sec = 0;		/* Reload value */
    timer.it_interval.tv_usec = 0;

    timer.it_value.tv_sec = tout;		/* One-Shot value */
    timer.it_value.tv_usec = 0;

    display (size, size0);
    while (1) {
	setitimer(ITIMER_REAL, &timer, 0);	/* reset timeout */

	n = status = fread (buffer, 1, BUFFER_SIZE, fp);
	if (status <= 0) 
	    break;
	display (size, n);
	status = fwrite (buffer, 1, n, file);
	if (status != n)
	    break;
    }
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = 0;
    setitimer(ITIMER_REAL, &timer, 0);		/* disable timeout */

    if (status < 0) 
	die(0);
    fclose(fp);
    fclose(file);
    display (size, -1);
    if (file != stdout)
	adjmodtime();
    exit (0);
}

void
display (int size, int n)
{
    static int bytes, pr, init = 0;
    static struct timeval t0, t_start;
    static char *s;
    struct timezone tz;
    struct timeval t;
    float d;
    
    if (!verbose)
	return;
    if (init == 0) {
	init = 1;
	gettimeofday(&t0, &tz);
	t_start = t0;
	bytes = pr = 0;
	s = (char *) malloc (strlen(outputfile) + 50);
	if (size > 0)
	    sprintf (s, "Receiving %s (%d bytes)%s", outputfile, size,
		     size ? "" : " [appending]");
	else
	    sprintf (s, "Receiving %s", outputfile);
	printf ("%s", s);
	fflush (stdout);
	bytes = n;
	return;
    }
    gettimeofday(&t, &tz);
    if (n == -1) {
	if (size > 0) 
	    printf ("\r%s: 100%%", s);
	else
	    printf ("\r%s: %d Kbytes", s, bytes/1024);
	d = t.tv_sec + t.tv_usec/1.e6 - t_start.tv_sec - t_start.tv_usec/1.e6;
	printf ("\n%d bytes transfered in %.1f seconds", bytes, d); 
	d = bytes/d;
	if (d < 1000)
	    printf ("  (%d Bytes/s)\n", (int)d);
	else {
	    d /=1024;
	    printf ("  (%.2f K/s)\n", d);
	}
	return;
    }
    bytes += n;
    d = t.tv_sec + t.tv_usec/1.e6 - t0.tv_sec - t0.tv_usec/1.e6;
    if (d < 5)		/* display every 5 sec. */
	return;
    t0 = t;
    pr++;
    if (size > 0) 
	printf ("\r%s: %2d%%", s, 100*bytes/size);
    else
	printf ("\r%s: %d Kbytes", s, bytes/1024);
    fflush (stdout);
}

void
parse (char *s)
{
    char *p;
    
    if (strncasecmp (s, "file:", 5) == 0) {
	/* file:filename */
	s += 4;
	*s++ = '\0';
	host = NULL;
	ftp = http = 0;
	file_to_get = s;
	return;
    }
    else if (strncasecmp (s, "ftp://", 6) == 0) {
	/* ftp://host.name/file/name */
	s += 6;
	p = strchr(s, '/');
	if (!p) {
	    warnx("no filename??");
	    usage();
	}
    }
    else if (strncasecmp (s, "http://", 7) == 0) {
	/* http://host.name/file/name */
	char *q;
	s += 7;
	p = strchr(s, '/');
	if (!p) {
	    warnx ("no filename??");
	    usage ();
	}
	*p++ = 0;
	q = strchr (s, ':');
	if (q && q < p) {
	    *q++ = 0;
	    http_port = atoi (q);
	}
	host = s;
	file_to_get = p;
	http = 1;
	return;
    }
    else {
	/* assume host.name:/file/name */
	p = strchr (s, ':');
	if (!p) {
	    /* assume /file/name */
	    host = NULL;
	    ftp = http = 0;
	    file_to_get = s;
	    return;
	}
    }
    ftp = 1;
    *p++ = 0;
    host = s;
    s = strrchr (p, '/');
    if (s) {
	*s++ = 0;
	change_to_dir = p;
	file_to_get = s;
    } else {
	change_to_dir = 0;
	file_to_get = p;
    }
}

void
output_file_name ()
{
    char *p;
    
    if (!outputfile) {
	p = strrchr (file_to_get, '/');
	if (!p || (!ftp && !http))
	    p = file_to_get;
	else
	    p++;
	outputfile = strdup (p);
    }
}

void
f_size (char *name, off_t *size, time_t *time)
{
    struct stat s;
    
    *size = 0;
    
    if (stat (name, &s))
	return;
    *size = s.st_size;
    *time = s.st_mtime;
}

void
ftperr (FILE* ftp, char *fmt, ...)
{
    va_list ap;
    va_start (ap, fmt);
    
    if (fmt)
	vfprintf(stderr, fmt, ap);
    if(ftp) {
	const char *str = ftpErrString(ftpErrno(ftp));

	if (str)
	    fprintf(stderr, "%s\n", str);
    }
    rm ();
    exit (1);
}

void
httpget ()
{
    char *cp, str[1000];
    struct timeval tv;
    time_t tout;
    fd_set fdset;
    int i, s;
    
    restart = 0;
    
    s = http_open ();
    sprintf (str, "GET %s%s HTTP/1.0\r\n\r\n", 
	     http_proxy? "" : "/", file_to_get);
    i = strlen (str);
    if (i != write (s, str, i))
	err (1, "could not send GET command to HTTP server.");
    
    FD_ZERO (&fdset);
    FD_SET (s, &fdset);
    if (timeout_ival)
	tout = timeout_ival;
    else if ((cp = getenv("HTTP_TIMEOUT")) != NULL)
	tout = atoi(cp);
    else
	tout = HTTP_TIMEOUT;
    
    if (strcmp (outputfile, "-")) {
	file = fopen (outputfile, "w");
	if (!file)
	    err (1, "could not open output file %s.", outputfile);
    } else {
	file = stdout;
	verbose = 0;
    }
    
    while (1) {
	tv.tv_sec = tout;
	tv.tv_usec = 0;
	i = select (s+1, &fdset, 0, 0, &tv); 
	switch (i) {
	case 0:
	    warnx ("Timeout");
	    rm ();
	    exit (1);
	case 1:
	    i = read (s, buffer, sizeof (buffer));
	    filter (buffer, i);
	    if (i == 0)
		exit (0);
	    break;
	default:
	    err (1, "communication error with HTTP server.");
	}
    }
}

int
match (char *pat, char *s)
{
    regex_t preg;
    regmatch_t pmatch[2];
    
    regcomp (&preg, pat, REG_EXTENDED|REG_ICASE);
    if (regexec(&preg, s, 2, pmatch, 0))
	return 0;
    return pmatch[1].rm_so ? pmatch[1].rm_so : -1;
}

void
filter (unsigned char *p, int len)
{
#define S 512
    static unsigned char s[S+2];
    static int header_len = 0, size = -1, n;
    int i = len;
    unsigned char *q = p;
    
    if (header_len < S) {
	while (header_len < S && i--)
	    s[header_len++] = *q++;
	s[header_len] = 0;
	if (len && (header_len < S))
	    return;
	if (match ("^HTTP/[0-9]+\\.[0-9]+[ \t]+200[^0-9]", s) == 0) {
	    /* maybe not found, or document w/o header */
	    if (match ("^HTTP/[0-9]+\\.[0-9]+[ \t]+[0-9]+", s)) {
		fprintf (stderr, "%s fetching failed, header so far:\n%s\n", file_to_get, s);
		rm ();
		exit (1);
	    }
	    /* assume no header */
	    /* write s */
	    display (size, 0);
	    i = fwrite (s, 1, header_len, file);
	    if (i != header_len)
		die(0);
	    display (size, header_len);
	    /* then p */
	    if (p+len > q) {
		i = fwrite (q, 1, p+len-q, file);
		if (i != p+len-q)
		    die(0);
		display (size, i);
	    }
	} else {
	    unsigned char *t;
	    /* document begins with a success line. try to get size */
	    i = match ("content-length:[ \t]*([0-9]+)", s);
	    if (i > 0)
		size = atoi (s+i);
	    /* assume that the file to get begins after an empty line */
	    i = match ("(\n\n|\r\n\r\n)", s);
	    if (i > 0) {
		if (s[i] == '\r')
		    t = s+i+4;
		else
		    t = s+i+2;
	    } else {
		fprintf (stderr, "Can't decode the header!\n");
		rm ();
		exit (1);
	    }
	    display (size, 0);
	    n = (s-t)+header_len;
	    i = fwrite (t, 1, n, file);
	    if (i != n)
		die(0);
	    display (size, n);
	    if (p+len > q) {
		n = p+len-q;
		i = fwrite (q, 1, n, file);
		if (i != n)
		    die(0);
		display (size, n);
	    }
	}
    } else {
	i = fwrite (p, 1, len, file);
	if (i != len) 
	    die(0);
	if (len)
	    display (size, i);
    }
    if (len == 0)
	display (size, -1);
}

int
http_open()
{
    unsigned long a;
    struct sockaddr_in sin, sin2;
    struct hostent *h;
    int s;
    
    a = inet_addr (host);
    if (a != INADDR_NONE) {
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = a;
    } else {
	h = gethostbyname (host);
	if (!h) 
	    err (1, "could not lookup host %s.", host);
	sin.sin_family = h->h_addrtype;
	bcopy(h->h_addr, (char *)&sin.sin_addr, h->h_length);
    }
    sin.sin_port = htons (http_port);
    if ((s = socket (sin.sin_family, SOCK_STREAM, 0)) < 0) 
	err (1, "socket");
    bzero ((char *)&sin2, sizeof (sin2));
    sin2.sin_family = AF_INET;
    sin2.sin_port = 0;
    sin2.sin_addr.s_addr = htonl (INADDR_ANY);
    if (bind (s, (struct sockaddr *)&sin2, sizeof (sin2)))
	err (1, "could not bind to socket.");
    
    if (connect(s, (struct sockaddr *)&sin, sizeof(sin)) < 0)
	err (1, "connection failed");
    return s;
}

int
isDebug ()
{
    return 0;
}

void msgDebug (char *p)
{
    printf ("%s", p);
}

void
setup_http_proxy()
{
    char *e;
    char *p;
    char *url;
    unsigned short port;
    
    if (!(e = getenv("HTTP_PROXY"))
	|| !(p = strchr(e, ':'))
	|| (port = atoi(p+1)) == 0)
	return;
    
    if (!(url = (char *) malloc (strlen(file_to_get) 
				 + strlen(host) 
				 + (change_to_dir ? strlen(change_to_dir) : 0)
				 + 50)))
	return;
    
    if (http) {
	sprintf(url, "http://%s:%d/%s",
		host, http_port, file_to_get);
    } else {
	if (change_to_dir) {
	    sprintf(url, "ftp://%s/%s/%s", 
		    host, change_to_dir, file_to_get);
	} else {
	    sprintf(url, "ftp://%s/%s", host, file_to_get);
	}
    }
    file_to_get = url;
    
    *p = 0;
    host = strdup(e);
    http_port = port;
    http = 1;
    http_proxy = 1;
}

