/* 
 * Copyright (C) 1996
 *   interface business GmbH
 *   Tolkewitzer Strasse 49
 *   D-01277 Dresden
 *   F.R. Germany
 *
 * All rights reserved.
 *
 * Written by Joerg Wunsch <joerg_wunsch@interface-business.de>
 * Modified by Luigi Rizzo <luigi@iet.unipi.it>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#ifndef lint
static const char rcsid[] =
	"$Id: wormcontrol.c,v 1.1.1.1.2.3 1997/11/18 07:28:15 charnier Exp $";
#endif /* not lint */

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include <sys/time.h>
#include <sys/errno.h>

#include <sys/ioctl.h>
#include <sys/cdio.h>
#include <sys/wormio.h>

/*
 * these two are in /sys/i386/isa/atapi-cd.h
 */
#define CDRIOCBLANK     _IO('c',100)    /* Blank a CDRW disc */
#define CDRIOCNEXTWRITEABLEADDR _IOR('c',101,int)

int nextwriteable = 0 ;

char
usage_string[] =
"usage: wormcontrol [options] file              (compact form)\n"
"usage: wormcontrol [-f device] command [args]  (traditional form)\n"
"-f device     specify input file\n"
"-d | -a       data or audio track (one required, unless doing -i)\n"
"-i            print disk info (tracks)\n"
"-e            do the final fixate\n"
"-n            open next session doing fixate\n"
"-s speed      select speed (single, double, quad, or 1,2,4; default double)\n"
"-p            preemphasis (only for audio)\n"
"-t            test mode\n"
"-v | -q       verbose or quiet mode\n"
;

static void
usage()
{
	fprintf(stderr,"%s", usage_string);
	exit(EX_USAGE);
}

#define eq(a, b) (strcmp(a, b) == 0)

struct atapires {
         u_char  code;
         u_char  status;
         u_char  error;
};

#define ATAPI_WRITE_BIG         0x2a    /* write data */

#define CDRIOCATAPIREQ  _IOWR('c',102,struct atapireq)

struct atapireq {
         u_char  cmd[16];
         caddr_t databuf;
         int     datalen;
         struct  atapires result;
} ;


int
info(int fd)
{
	struct ioc_toc_header h;
	struct ioc_read_toc_entry t;
	struct cd_toc_entry toc_buffer[100];
	int ntocentries, i;

	bzero(&h, sizeof(h));
	if (ioctl(fd, CDIOREADTOCHEADER, &h) == -1) {
		perror("CDIOREADTOCHEADER");
		return -1;
	}

	ntocentries = h.ending_track - h.starting_track + 1;
	fprintf(stderr, "%d tracks, start %d, len %d\n",
		ntocentries,
		h.starting_track, h.ending_track);
	if (ntocentries > 100) {
		/* unreasonable, only 100 allowed */
		return -1;
	}
	t.address_format = CD_LBA_FORMAT;
	t.starting_track = 0;
	t.data_len = (1+ntocentries) * sizeof(struct cd_toc_entry);
	t.data = toc_buffer;
	bzero(toc_buffer, sizeof(toc_buffer));

	if (ioctl(fd, CDIOREADTOCENTRYS, (char *) &t) == -1) {
		perror("CDIOREADTOCENTRYS");
		return -1;
	}
	fprintf(stderr,
"track     block  length   type\n"
"-------------------------------\n");
	for (i = 0 ; i < ntocentries ; i++) {
	    fprintf(stderr, "%5d  %6d  %6d  %5s\n",
		toc_buffer[i].track,
		ntohl(toc_buffer[i].addr.lba),
		ntohl(toc_buffer[i+1].addr.lba)-ntohl(toc_buffer[i].addr.lba),
		toc_buffer[i].control & 4 ? "data" : "audio"
	    );
	}
#if 0
	for (i = ntocentries - 1; i >= 0; i--)
		if ((toc_buffer[i].control & 4) != 0)
			/* found a data track */
			break;
	if (i < 0)
		return -1;
	return ntohl(toc_buffer[i].addr.lba);
#endif
}

int
do_nextwriteable(int fd)
{
    int addr = 0;
    int err ;
    if (ioctl(fd, CDRIOCNEXTWRITEABLEADDR, &addr) == -1) {
	fprintf(stderr, "nextwriteable error\n", errno);
	nextwriteable = 0 ;
    } else
	nextwriteable = addr ;
    fprintf(stderr, "Next writeable at LBA %d (0x%x)\n", addr , addr);
    return 0 ;
}

int
do_prepdisk(int fd, int dummy, int speed)
{
    struct wormio_prepare_disk d;

    fprintf(stderr, "--- prepdisk %s %s ---\n",
	speed == 1 ? "single" : "double",
	dummy ? "dummy" : "real ");

    d.dummy = dummy;
    d.speed = speed;
    if (ioctl(fd, WORMIOCPREPDISK, &d) == -1)
	err(EX_IOERR, "ioctl(WORMIOCPREPDISK)");
    return 0 ;
}

int
do_fixate(int fd, int type, int onp)
{
    struct wormio_fixation f;

    f.toc_type = type ;
    f.onp = onp ;

    fprintf(stderr, "--- fixate %d%s ---\n", type, onp ? " onp":"");
    if (ioctl(fd, WORMIOCFIXATION, &f) == -1)
	err(EX_IOERR, "ioctl(WORMIOFIXATION)");
}

int
write_data(int fd, char *fn, int audio)
{
    int src;
    char buf[8*2352];
    int l ;
    u_long tot = 0 ;
    int sz, bsz ;
    int startlba ;
    int retry = 1 ;

    struct timeval beg, old, end ;

    fprintf(stderr, "--- Writing <%s> %s ---\n", fn, audio ? "AUDIO":"DATA");

retry :
    if (eq (fn, "-") )
	src = 0 ;
    else
	src = open(fn, O_RDONLY);
    if (src < 0) {
	fprintf(stderr, "Can't open %s\n", fn);
	return ;
    }
    bsz = (audio) ? 2352 : 2048 ;
    sz = 8*bsz ;
    startlba = nextwriteable ; /* the device should know... */
    gettimeofday(&beg, NULL);
    old = beg ;
    end = beg ;

    while ( ( l = read(src, buf, sz) ) > 0 ) {
	int l1 = l % bsz ;
	if ( l1 != 0 ) {
	    printf("Warning, file size not multiple of %d, trimming %d\n",
		 bsz, l1);
	    l -= l1 ;
	}
	tot += l ;

	if (l) {
	    /* ioctl to write... */

	    int nblocks = l / bsz ;
	    struct atapireq ar;

	    bzero(&ar, sizeof ar);
	    ar.cmd[0] = ATAPI_WRITE_BIG;
	    ar.cmd[1] = 0; /* was 0 */
	    ar.cmd[2] = startlba >> 24;
	    ar.cmd[3] = startlba >> 16;
	    ar.cmd[4] = startlba >> 8;
	    ar.cmd[5] = startlba;
	    ar.cmd[6] = 0;
	    ar.cmd[7] = nblocks >> 8;
	    ar.cmd[8] = nblocks ;
	    /* others are zero */
	    ar.databuf = buf;
	    ar.datalen = l ;
	    if (ioctl(fd, CDRIOCATAPIREQ, &ar) < 0) {
		/*
		 * the first write might fail when changing from
		 * data to audio tracks and vice-versa.
		 * This is non-fatal if we are at the beginning and
		 * we are not reading from stdin, so we retry once.
		 *
		 * The driver ought to be fixed of course...
		 */
		tot -= l ;
		if (retry-- > 0 && tot == 0 && src != 0) {
		    fprintf(stderr,
			"--- non fatal error on first write, retry...\n");
		    do_nextwriteable(fd);
		    close(src);
		    goto retry ;
		}
		fprintf(stderr,
		    "\n--- FATAL: Failure writing %d blocks at %d\n",
			nblocks, tot);
		perror("CDRIOATAPIREQ");
		goto done ;
	    }
	    gettimeofday(&end, NULL);
	    if (startlba == 0) {
		/* the first write is slow so we do not count it... */
		beg = end ;
		old = end ;
	    }
	    startlba += nblocks ;
	    l1 = end.tv_sec - old.tv_sec ;
	    l = end.tv_sec - beg.tv_sec ;
	    if (l1) {
		old = end ;
		fprintf(stderr, "%d KB (%d KB/s)          \r",
			 tot/1024, tot/ (1024*l )  );
	    }
	}
    }
done:
    l = end.tv_sec - beg.tv_sec ;
    fprintf(stderr, "\n\nWritten %d KB in %d s (%d KB/s)\n",
		tot/1024, l, tot/(1024* (l ? l : 1 ) ) ) ;
}

int
do_track(int fd, char *fn, int audio, int preemp)
{
    struct wormio_prepare_track t;		

    if (audio == 0 && preemp == 1)
	errx(EX_USAGE, "\"preemp\" attempted on data track");
    t.audio = audio ;
    t.preemp = preemp ;

    if (ioctl(fd, WORMIOCPREPTRACK, &t) == -1)
	err(EX_IOERR, "ioctl(WORMIOCPREPTRACK)");
    do_nextwriteable(fd);
    if (fn)
	write_data(fd, fn, t.audio);
}


/*** Parameter		default value			*/
char *fn = NULL ;
int idx = 0 ;		/* normal operation, use -i to list content	*/
int fixate = 0 ;	/* do not fixate, use -e to force it		*/
int compact = 0 ;	/* standard command format			*/
int audio = -1 ;	/* unknown data format, need -d or -a		*/
int verbose = 1 ;	/* standard verbosity level			*/
int onp = 0 ;		/* don't open next session, use -n to force it	*/
int dummy = 0 ;		/* regular write, use -d to do a dummy write	*/
int speed = 2 ;		/* default: double speed			*/
int preemp = 0 ;	/* default: no preemphasis, use -p to enable it	*/

int
main(int argc, char **argv)
{
	int fd, c, i;
	int errs = 0;
    char *devname ;

    devname = getenv("WORMDEVICE");

    if (devname == NULL)
	devname = "/dev/rworm0" ;

    while ((c = getopt(argc, argv, "f:qvadntps:ei")) !=  -1)
		switch(c) {
	case 'i':
	    idx = 1 ;
	    compact = 1 ;
	    break ;

		case 'f':
			devname = optarg;
			break;

	case 'q':
	    compact = 1 ;
	    verbose = 0 ;
	    break ;

	case 'v':
	    compact = 1 ;
	    verbose = 2 ;
	    break ;

	case 'a' :
	    compact = 1 ;
	    audio = 1 ;
	    preemp = 1 ;
	    break ;

	case 'd' :
	    compact = 1 ;
	    audio = 0 ;
	    preemp = 0 ;
	    break ;

	case 'p' :
	    compact = 1 ;
	    preemp = 1 ;
	    break ;

	case 's' :
	    compact = 1 ;
	    if (eq(optarg,"single") || eq(optarg,"1"))
		speed = 1 ;
	    else if (eq(optarg,"double") || eq(optarg,"2"))
		speed = 2 ;
	    else if (eq(optarg,"quad") || eq(optarg,"4"))
		speed = 4 ;
	    else
		usage();
	    break ;

	case 'e' :
	    compact = 1 ;
	    fixate = 1 ;
	    break ;

	case 'n' :
	    compact = 1 ;
	    onp = 1;
	    break ;

	case 't' :
	    dummy = 1 ;
	    compact = 1 ;
	    break ;

		default:
			errs++;
		}
	
	argc -= optind;
	argv += optind;

    if ((fd = open(devname, O_RDONLY /* | O_NONBLOCK */, 0)) == -1)
	    err(EX_NOINPUT, "open(%s)", devname);
    if (idx) {
	info(fd);
	exit(0);
    }
    if (compact && audio < 0) /* bad for compact format */
	usage() ;
    if (errs || argc < 1) /* bad for traditional format */
		usage();


    if (compact) {
	fn = argv[0];
	do_prepdisk(fd, dummy, speed);
	do_track(fd, fn, audio, preemp);
	if (fixate)
	    do_fixate(fd, audio ? 0 : 1 , onp ? 1 : 0);
	return 0 ;
    } else {
	if (eq(argv[0], "select")) {
		struct wormio_quirk_select q;
		if (argc != 3)
			errx(EX_USAGE, "wrong params for \"select\"");
		q.vendor = argv[1];
		q.model = argv[2];
		if (ioctl(fd, WORMIOCQUIRKSELECT, &q) == -1)
			err(EX_IOERR, "ioctl(WORMIOCQUIRKSELECT)");
	} else if (eq(argv[0], "prepdisk")) {
	    speed = -1 ;
		for (i = 1; i < argc; i++) {
			if (eq(argv[i], "dummy"))
		    dummy = 1;
			else if (eq(argv[i], "single"))
		    speed = 1;
			else if (eq(argv[i], "double"))
		    speed = 2;
			else
		    errx(EX_USAGE, "wrong param for \"prepdisk\": %s", argv[i]);
		}
	    if (speed == -1)
			errx(EX_USAGE, "missing speed parameter");
	    do_prepdisk(fd, dummy, speed);
	} else if (eq(argv[0], "endtrack")) {
	    if (ioctl(fd, WORMIOCFINISHTRACK, NULL) == -1)
		err(EX_IOERR, "ioctl(WORMIOCFINISHTRACK)");
	} else if (eq(argv[0], "track")) {
		struct wormio_prepare_track t;		
	    fn = NULL ;
	    audio = -1;
	    preemp = 0;
		for (i = 1; i < argc; i++) {
			if (eq(argv[i], "audio"))
		    audio = 1;
			else if (eq(argv[i], "data"))
		    audio = 0;
			else if (eq(argv[i], "preemp"))
		    preemp = 1;
		else if (i == argc-1)
		    fn = argv[i];
			else
		    errx(EX_USAGE, "wrong param for \"track\": %s", argv[i]);
		}
	    if (audio == -1)
			errx(EX_USAGE, "missing track type parameter");
	    do_track(fd, fn, audio, preemp);
	} else if (eq(argv[0], "fixate")) {
	    int type = -1 ;
	    onp = 0;
		for (i = 1; i < argc; i++) {
			if (eq(argv[i], "onp"))
		    onp = 1;
		else if (eq(argv[i], "audio") )
		    type = 0 ;
		else if (eq(argv[i], "cdda") )
		    type = 0 ;
		else if (eq(argv[i], "data") )
		    type = 1 ;
		else if (eq(argv[i], "cdrom") )
		    type = 1 ;
			else if (argv[i][0] >= '0' && argv[i][0] <= '4' &&
				 argv[i][1] == '\0')
		    type = argv[i][0] - '0';
			else
		    errx(EX_USAGE, "wrong param for \"fixate\": %s", argv[i]);
		}
	    if (type == -1)
			errx(EX_USAGE, "missing TOC type parameter");
	    do_fixate(fd, type, onp);
        } else if (eq(argv[0], "blank")) {
	    if (ioctl(fd, CDRIOCBLANK) == -1)
		err(EX_IOERR, "ioctl(CDRIOCBLANK)");
        } else if (eq(argv[0], "nextwriteable")) {
	    do_nextwriteable(fd);
	} else
		errx(EX_USAGE, "unknown command: %s", argv[0]);

	return EX_OK;
    }
}
