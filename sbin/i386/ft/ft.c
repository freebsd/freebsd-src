/*
 *  Copyright (c) 1993 Steve Gerakines
 *
 *  This is freely redistributable software.  You may do anything you
 *  wish with it, so long as the above notice stays intact.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS
 *  OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT,
 *  INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 *  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 *  STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 *  IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 *  ft.c - simple floppy tape filter
 *
 *  01/28/94 v0.3b (Jim Babb)
 *  Fixed bug when all sectors in a segment are marked bad.
 *
 *  10/30/93 v0.3
 *  Minor revisions.  Seems pretty stable.
 *
 *  09/02/93 v0.2 pl01
 *  Initial revision.
 *
 *  usage: ftfilt [ -f tape ] [ description ]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <sys/ftape.h>

#define	DEFQIC	"/dev/rft0"

char buff[QCV_SEGSIZE];			/* scratch buffer */
char hbuff[QCV_SEGSIZE];		/* header buffer */
QIC_Header *hptr = (QIC_Header *)hbuff;	/* header structure */
int hsn = -1;				/* segment number of header */
int dhsn = -1;				/* segment number of duplicate header */
int tfd;				/* tape file descriptor */
QIC_Geom geo;				/* tape geometry */
int tvno = 1;				/* tape volume number */
int tvlast;				/* TRUE if last volume in set */
long tvsize = 0;			/* tape volume size in bytes */
long tvtime = NULL;			/* tape change time */
char *tvnote = "";			/* tape note */

/* Lookup the badmap for a given track and segment. */
#define BADMAP(t,s)	hptr->qh_badmap[(t)*geo.g_segtrk+(s)]

/* Retrieve values from a character array. */
#define UL_VAL(s,p)	(*((ULONG *)&(s)[p]))
#define US_VAL(s,p)	(*((USHORT *)&(s)[p]))

#define	equal(s1,s2)	(strcmp(s1, s2) == 0)



/* Entry */
main(int argc, char *argv[])
{
  int r, s;
	char *tape, *getenv();

	if (argc > 2 && (equal(argv[1], "-t") || equal(argv[1], "-f"))) {
		argc -= 2;
		tape = argv[2];
		argv += 2;
	} else
		if ((tape = getenv("TAPE")) == NULL)
			tape = DEFQIC;
  if (argc > 1) {
	tvnote = argv[1];
	if (strlen(tvnote) > 18) argv[1][18] = '\0';
  }

  /* Open the tape device */
  if ((tfd = open(tape, 2)) < 0) {
	perror(tape);
	exit(1);
  }

  if (!isatty(0))
	do_write();
  else if (!isatty(1))
	do_read();
  else
	do_getname();

  close(tfd);
  exit(0);
}


/* Check status of tape drive */
int check_stat(int fd, int wr)
{
  int r, s;
  int sawit = 0;

  /* get tape status */
  if (ioctl(fd, QIOSTATUS, &s) < 0) {
	fprintf(stderr, "could not get drive status\n");
	return(1);
  }

  /* wait for the tape drive to become ready */
  while ((s & QS_READY) == 0) {
	if (!sawit) {
		fprintf(stderr, "waiting for drive to become ready...\n");
		sawit = 1;
	}
	sleep(2);
	if (ioctl(fd, QIOSTATUS, &s) < 0) {
		fprintf(stderr, "could not get drive status\n");
		return(1);
	}
  }
 
  if ((s & QS_FMTOK) == 0) {
	fprintf(stderr, "tape is not formatted\n");
	return(2);
  }

  if (wr && (s & QS_RDONLY) != 0) {
	fprintf(stderr, "tape is write protected\n");
	return(3);
  }

  return(0);
}



ULONG qtimeval(time_t t)
{
  struct tm *tp;
  ULONG r;

  tp = localtime(&t);
  r = 2678400 * tp->tm_mon +
	86400 *(tp->tm_mday-1) +
	 3600 * tp->tm_hour +
	   60 * tp->tm_min +
	        tp->tm_sec;
  r |= (tp->tm_year - 70) << 25;
  return(r);
}

/* Return tm struct from QIC date format. */
struct tm *qtime(UCHAR *qt)
{
  ULONG *vp = (ULONG *)qt;
  struct tm t;
  ULONG v;
  time_t tv;

  v = *vp;
  t.tm_year = ((v >> 25) & 0x7f)+70; v &= 0x1ffffff;
  t.tm_mon = v / 2678400; v %= 2678400;
  t.tm_mday = v / 86400 + 1; v %= 86400;
  t.tm_hour = v / 3600; v %= 3600;
  t.tm_min = v / 60; v %= 60;
  t.tm_sec = v;
  t.tm_wday = 0;	/* XXX - let mktime do the real work */
  t.tm_yday = 0;
  t.tm_isdst = 0;
  t.tm_gmtoff = 0;
  t.tm_zone = NULL;
  tv = mktime(&t);
  return(localtime(&tv));
}

/* Return a string, zero terminated */
char *qstr(char *str, int nchar)
{
  static char tstr[256];
  strncpy(tstr, str, nchar);
  tstr[nchar] = '\0';
  return(tstr);
}

/* Read header from tape */
get_header(int fd)
{
  int r, sn, bytes;
  QIC_Segment s;
  int gothdr = 0;

  if (ioctl(fd, QIOGEOM, &geo) < 0) {
	fprintf(stderr, "couldn't determine tape geometry\n");
	return(1);
  }

  /* Get the header and duplicate */
  for (sn = 0; sn < 16; sn++) {
	s.sg_trk = 0;
	s.sg_seg = sn;
	s.sg_badmap = 0;
	s.sg_data = (UCHAR *)&buff[0];
	ioctl(fd, QIOREAD, &s);
	r = check_parity(s.sg_data, 0, s.sg_crcmap);
	if (s.sg_data[0] == 0x55 && s.sg_data[1] == 0xaa &&
	    s.sg_data[2] == 0x55 && s.sg_data[3] == 0xaa) {
		if (hsn >= 0) {
			dhsn = sn;
			if (!r && !gothdr) {
				fprintf(stderr, "using secondary header\n");
				bcopy(s.sg_data, hbuff, QCV_SEGSIZE);
				gothdr = 1;
			}
			break;
		}
		hsn = sn;
		if (!r) {
			bcopy(s.sg_data, hbuff, QCV_SEGSIZE);
			gothdr = 1;
		} else {
			fprintf(stderr, "too many errors in primary header\n");
		}
	}
  }

  if (!gothdr) {
	fprintf(stderr, "couldn't read header segment\n");
	ioctl(fd, QIOREWIND);
	return(1);
  }

  return(0);
}


ask_vol(int vn)
{
  FILE *inp;
  int fd;
  char c;

  if ((fd = open("/dev/tty", 2)) < 0) {
	fprintf(stderr, "argh!! can't open /dev/tty\n");
	exit(1);
  }

  fprintf(stderr, "Insert ftfilt volume %02d and press enter:", vn);
  read(fd, &c, 1);
  close(fd);
}


/* Return the name of the tape only. */
do_getname()
{
  if (check_stat(tfd, 0)) exit(1);
  if (get_header(tfd)) exit(1);
  fprintf(stderr, "\"%s\" - %s",
		qstr(hptr->qh_tname,44), asctime(qtime(hptr->qh_chgdate)));
  ioctl(tfd, QIOREWIND);
}


/* Extract data from tape to stdout */
do_read()
{
  int sno, vno, sbytes, r;
  long curpos;
  char *hname;
  QIC_Segment s;

  /* Process multiple volumes if necessary */
  vno = 1;
  for (;;) {
	if (check_stat(tfd, 0)) {
		ask_vol(vno);
		continue;
	}
	if (get_header(tfd)) {
		ask_vol(vno);
		continue;
	}

	/* extract volume and header info from label */
	hname = hptr->qh_tname;
	hname[43] = '\0';
	tvno = atoi(&hname[11]);
	tvlast = (hname[10] == '*') ? 1 : 0;
	tvsize = atoi(&hname[14]);
	tvnote = &hname[25];
	if (vno != tvno || strncmp(hname, "ftfilt", 6) != 0) {
		fprintf(stderr, "Incorrect volume inserted.  This tape is:\n");
		fprintf(stderr,"\"%s\" - %s\n", hname,
				asctime(qtime(hptr->qh_chgdate)));
		ask_vol(vno);
		continue;
	}

	/* Process this volume */
	curpos = 0;
	for (sno = hptr->qh_first; tvsize > 0; sno++) {
		s.sg_trk = sno / geo.g_segtrk;
		s.sg_seg = sno % geo.g_segtrk;
		s.sg_badmap = BADMAP(s.sg_trk,s.sg_seg);
		sbytes = sect_bytes(s.sg_badmap) - QCV_ECCSIZE;
		s.sg_data = (UCHAR *)&buff[0];

		/* skip segments with *all* sectors flagged as bad */
		if (sbytes > 0) {
			if (ioctl(tfd, QIOREAD, &s) < 0) perror("QIOREAD");
			r = check_parity(s.sg_data, s.sg_badmap, s.sg_crcmap);
			if (r) fprintf(stderr, "** warning: ecc failed at byte %ld\n",
									 curpos);
			if (tvsize < sbytes) sbytes = tvsize;
			write(1, s.sg_data, sbytes);
			tvsize -= sbytes;
			curpos += sbytes;
		}
	}
	if (tvlast) break;
	ioctl(tfd, QIOREWIND);
	ask_vol(++vno);
  }
  ioctl(tfd, QIOREWIND);
  return(0);
}


/* Dump data from stdin to tape */
do_write()
{
  int sno, vno, amt, sbytes;
  int c, maxseg, r;
  ULONG qnow;
  QIC_Segment s;
  char tmpstr[80];

  qnow = qtimeval(time(NULL));
  vno = 1;

  for (;;) {
	if (check_stat(tfd, 1)) {
		ask_vol(vno);
		continue;
	}
	if (get_header(tfd)) {
		ask_vol(vno);
		continue;
	}

	maxseg = geo.g_segtrk * geo.g_trktape - 1;
	sno = hptr->qh_first;
	tvno = vno;
	tvsize = 0;
	tvlast = 0;

	/* Process until end of volume or end of data */
	for (sno = hptr->qh_first; sno < maxseg && tvlast == 0; ++sno) {
		/* Prepare to load the next segment */
		s.sg_trk = sno / geo.g_segtrk;
		s.sg_seg = sno % geo.g_segtrk;
		s.sg_badmap = BADMAP(s.sg_trk,s.sg_seg);
		sbytes = sect_bytes(s.sg_badmap) - QCV_ECCSIZE;
		s.sg_data = (UCHAR *)&buff[0];

		/* Ugh.  Loop to get the full amt. */
		for (amt = 0; amt < sbytes; amt += r) {
			r = read(0, &s.sg_data[amt], sbytes - amt);
			if (r <= 0) {
				tvlast = 1;
				break;
			}
		}
		/* skip the segment if *all* sectors are flagged as bad */
		if (amt) {
			if (amt < sbytes)
				bzero(&s.sg_data[amt], sbytes - amt);
			r = set_parity(s.sg_data, s.sg_badmap);
			if (r) fprintf(stderr, "** warning: ecc problem !!\n");
			if (ioctl(tfd, QIOWRITE, &s) < 0) {
				perror("QIOWRITE");
				exit(1);
			}
			tvsize += amt;
		}
	}

	/* Build new header info */
	/* ftfilt vol*xx yyyyyyyyyy note56789012345678  */
	/* 01234567890123456789012345678901234567890123 */

	sprintf(tmpstr, "ftfilt vol%s%02d %010d %s",
		(tvlast) ? "*" : " ", tvno, tvsize, tvnote);
	strncpy(hptr->qh_tname, tmpstr, 44);
	UL_VAL(hptr->qh_chgdate,0) = qnow;

	/* Update the header for this volume */
	if (hsn >= 0) {
		s.sg_trk = hsn / geo.g_segtrk;
		s.sg_seg = hsn % geo.g_segtrk;
		s.sg_badmap = 0;
		s.sg_data = (UCHAR *)hbuff;
		r = set_parity(s.sg_data, s.sg_badmap);
		if (r) fprintf(stderr, "** warning: header ecc problem !!\n");
		if (ioctl(tfd, QIOWRITE, &s) < 0) {
			perror("QIOWRITE");
			exit(1);
			}
	}
	if (dhsn >= 0) {
		s.sg_trk = dhsn / geo.g_segtrk;
		s.sg_seg = dhsn % geo.g_segtrk;
		s.sg_badmap = 0;
		s.sg_data = (UCHAR *)hbuff;
		r = set_parity(s.sg_data, s.sg_badmap);
		if (r) fprintf(stderr, "** warning: duphdr ecc problem !!\n");
		if (ioctl(tfd, QIOWRITE, &s) < 0) {
			perror("QIOWRITE");
			exit(1);
			}
	}
	ioctl(tfd, QIOREWIND);
	if (tvlast) break;
	ask_vol(++vno);
  }
  return(0);
}
