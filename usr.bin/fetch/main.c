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

/* $Id: main.c,v 1.26.2.7 1997/08/19 01:58:37 asami Exp $ */

#include <sys/types.h>

#include <err.h>
#include <errno.h>
#include <limits.h>		/* needed for INT_MAX */
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include <sys/param.h>		/* for MAXHOSTNAMELEN */
#include <sys/time.h>		/* for struct timeval, gettimeofday */

#include "fetch.h"

static struct fetch_state clean_fetch_state;
static sigjmp_buf sigbuf;
static int get(struct fetch_state *volatile fs);

static void
usage()
{
	fprintf(stderr, "%s\n%s\n", 
		"usage: fetch [-DHILMNPRTValmnpqrv] [-o outputfile]",
		"             [-f file -h host [-c dir] | URL]");
	exit(EX_USAGE);
}


int
main(int argc, char *const *argv)
{
    int c;
    char *ep;
    struct fetch_state fs;
    const char *change_to_dir, *file_to_get, *hostname;
    int error, rv;
    unsigned long l;

    init_schemes();
    fs = clean_fetch_state;
    fs.fs_verbose = 1;
    change_to_dir = file_to_get = hostname = 0;

    while ((c = getopt(argc, argv, "abc:D:f:h:HilLmMnNo:pPqRrtT:vV:")) != -1) {
	    switch (c) {
	    case 'D': case 'H': case 'I': case 'N': case 'L': case 'V': 
		    break;	/* ncftp compatibility */
	    
	    case 'a':
		    fs.fs_auto_retry = 1;
		    break;

	    case 'b':
		    fs.fs_linux_bug = 1;
		    break;

	    case 'c':
		    change_to_dir = optarg;
		    break;

	    case 'f':
		    file_to_get = optarg;
		    break;
	    
	    case 'h':
		    hostname = optarg;
		    break;

	    case 'l':
		    fs.fs_linkfile = 1;
		    break;

	    case 'm': case 'M':
		    fs.fs_mirror = 1;
		    break;
	    
	    case 'n':
		    fs.fs_newtime = 1;
		    break;
	    
	    case 'o':
		    fs.fs_outputfile = optarg;
		    break;

	    case 'p': case 'P':
		    fs.fs_passive_mode = 1;
		    break;
	    
	    case 'q': 
		    fs.fs_verbose = 0;
		    break;

	    case 'r':
		    fs.fs_restart = 1;
		    break;

	    case 'R':
		    fs.fs_precious = 1;
		    break;
	    
	    case 't':
		    fs.fs_use_connect = 1;
		    break;

	    case 'T':
		    /* strtol sets errno to ERANGE in the case of overflow */
		    errno = 0;
		    l = strtoul(optarg, &ep, 0);
		    if (!optarg[0] || *ep || errno != 0 || l > INT_MAX)
			    errx(EX_USAGE, "invalid timeout value: `%s'", 
				 optarg);
		    fs.fs_timeout = l;
		    break;

	    case 'v':
		    if (fs.fs_verbose < 2)
			    fs.fs_verbose = 2;
		    else
			    fs.fs_verbose++;
		    break;

	    default: 	
	    case '?':
		    usage();
	    }
    }

    clean_fetch_state = fs; /* preserve option settings */

    if (argv[optind] && (hostname || change_to_dir || file_to_get)) {
	    warnx("cannot use -h, -c, or -f with a URI argument");
	    usage();
    }

    if (fs.fs_mirror && fs.fs_restart)
	errx(EX_USAGE, "-m and -r are mutually exclusive.");
    
    if (argv[optind] == 0) {
	    char *uri;

	    if (hostname == 0) hostname = "localhost";
	    if (change_to_dir == 0) change_to_dir = "";
	    if (file_to_get == 0) {
		    usage();
	    }

	    uri = alloca(sizeof("ftp://") + strlen(hostname) + 
			 strlen(change_to_dir) + 2 + strlen(file_to_get));
	    strcpy(uri, "ftp://");
	    strcat(uri, hostname);
	    /*
	     * XXX - we should %-map a leading `/' into `%2f', but for
	     * anonymous FTP it is unlikely to matter.  Still, it would
	     * be better to follow the spec.
	     */
	    if (change_to_dir[0] != '/')
		    strcat(uri, "/");
	    strcat(uri, change_to_dir);
	    if (file_to_get[0] != '/' && uri[strlen(uri) - 1] != '/')
		    strcat(uri, "/");
	    strcat(uri, file_to_get);

	    error = parse_uri(&fs, uri);
	    if (error)
		    return error;
	    return get(&fs);
    }

    for (rv = 0; argv[optind] != 0; optind++) {
	    error = parse_uri(&fs, argv[optind]);
	    if (error) {
		    rv = error;
		    continue;
	    }

	    error = get(&fs);
	    if (error) {
		    rv = error;
	    }
	    fs = clean_fetch_state;
    }
    return rv;
}
    
/*
 * The signal handling is probably more complex than it needs to be,
 * but it doesn't cost a lot, so we'll be extra-careful.  Using
 * siglongjmp() to get out of the signal handler allows us to
 * call rm() without having to store the state variable in some global
 * spot where the signal handler can get at it.  We also obviate the need
 * for a separate timeout signal handler.
 */
static int
get(struct fetch_state *volatile fs)
{
	volatile int error;
	struct sigaction oldhup, oldint, oldquit, oldterm;
	struct sigaction catch;
	volatile sigset_t omask;

	sigemptyset(&catch.sa_mask);
	sigaddset(&catch.sa_mask, SIGHUP);
	sigaddset(&catch.sa_mask, SIGINT);
	sigaddset(&catch.sa_mask, SIGQUIT);
	sigaddset(&catch.sa_mask, SIGTERM);
	sigaddset(&catch.sa_mask, SIGALRM);
	catch.sa_handler = catchsig;
	catch.sa_flags = 0;

	sigprocmask(SIG_BLOCK, &catch.sa_mask, (sigset_t *)&omask);
	sigaction(SIGHUP, &catch, &oldhup);
	sigaction(SIGINT, &catch, &oldint);
	sigaction(SIGQUIT, &catch, &oldquit);
	sigaction(SIGTERM, &catch, &oldterm);

	error = sigsetjmp(sigbuf, 0);
	if (error == SIGALRM) {
		rm(fs);
		unsetup_sigalrm();
		fprintf(stderr, "\n"); /* just in case */
		warnx("%s: %s: timed out", fs->fs_outputfile, fs->fs_status);
		goto close;
	} else if (error) {
		rm(fs);
		fprintf(stderr, "\n"); /* just in case */
		warnx("%s: interrupted by signal: %s", fs->fs_status, 
		      sys_signame[error]);
		sigdelset(&omask, error);
		signal(error, SIG_DFL);
		sigprocmask(SIG_SETMASK, (sigset_t *)&omask, 0);
		raise(error);	/* so that it gets reported as such */
	}

	sigprocmask(SIG_SETMASK, (sigset_t *)&omask, 0);
	error = fs->fs_retrieve(fs);

close:
	sigaction(SIGHUP, &oldhup, 0);
	sigaction(SIGINT, &oldint, 0);
	sigaction(SIGQUIT, &oldquit, 0);
	sigaction(SIGTERM, &oldterm, 0);
	fs->fs_close(fs);

	return error;
}


/*
 * Utility functions
 */

/*
 * Handle all signals by jumping back into get().
 */
void
catchsig(int sig)
{
	siglongjmp(sigbuf, sig);
}

/* Used to generate the progress display when not in quiet mode. */
void
display(struct fetch_state *fs, off_t size, ssize_t n)
{
    static off_t bytes;
    static off_t bytestart;
    static int pr, stdoutatty, init = 0;
    static struct timeval t0, t_start;
    static char *s;
    struct timezone tz;
    struct timeval t;
    float d;
    
    if (!fs->fs_verbose)
	return;
    if (init == 0) {
	init = 1;
	gettimeofday(&t0, &tz);
	t_start = t0;
	bytes = pr = 0;
	stdoutatty = isatty(STDOUT_FILENO);
	if (size > 0)
	    asprintf (&s, "Receiving %s (%qd bytes)%s", fs->fs_outputfile,
		     (quad_t)size,
		     size ? "" : " [appending]");
	else
	    asprintf (&s, "Receiving %s", fs->fs_outputfile);
	fprintf (stderr, "%s", s);
	bytestart = bytes = n;
	return;
    }
    gettimeofday(&t, &tz);
    if (n == -1) {
	if(stdoutatty) {
	    if (size > 0) 
		fprintf (stderr, "\r%s: 100%%", s);
	    else
		fprintf (stderr, "\r%s: %qd Kbytes", s, (quad_t)bytes/1024);
	}
	bytes -= bytestart;
	d = t.tv_sec + t.tv_usec/1.e6 - t_start.tv_sec - t_start.tv_usec/1.e6;
	fprintf (stderr, "\n%qd bytes transfered in %.1f seconds",
	    (quad_t)bytes, d); 
	d = bytes/d;
	if (d < 1000)
	    fprintf (stderr, "  (%.0f bytes/s)\n", d);
	else {
	    d /=1024;
	    fprintf (stderr, "  (%.2f Kbytes/s)\n", d);
	}
	free(s);
	init = 0;
	return;
    }
    bytes += n;
    d = t.tv_sec + t.tv_usec/1.e6 - t0.tv_sec - t0.tv_usec/1.e6;
    if (d < 5)		/* display every 5 sec. */
	return;
    t0 = t;
    pr++;
    if(stdoutatty) {
	if (size > 1000000) 
	    fprintf (stderr, "\r%s: %2qd%%", s, (quad_t)bytes/(size/100));
	else if (size > 0) 
	    fprintf (stderr, "\r%s: %2qd%%", s, (quad_t)100*bytes/size);
	else
	    fprintf (stderr, "\r%s: %qd Kbytes", s, (quad_t)bytes/1024);
    }
}

