/*-
 * Copyright (c) 1985, 1986, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1985, 1986, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)acucntrl.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */

/*  acucntrl - turn around tty line between dialin and dialout
 * 
 * Usage:	acucntrl {enable,disable} /dev/ttydX
 *
 * History:
 *	First written by Allan Wilkes (fisher!allan)
 *
 *	Modified June 8,1983 by W.Sebok (astrovax!wls) to poke kernel rather
 * 	than use kernel hack to turn on/off modem control, using subroutine
 *	stolen from program written by Tsutomu Shimomura
 *	{astrovax,escher}!tsutomu
 *
 *	Worked over many times by W.Sebok (i.e. hacked to death)
 *
 * Operation:
 *   disable (i.e. setup for dialing out)
 *	(1) check input arguments
 *	(2) look in _PATH_UTMP to check that the line is not in use by another
 *	(3) disable modem control on terminal
 *	(4) check for carrier on device
 *	(5) change owner of device to real id
 *	(6) edit _PATH_TTYS, changing the first character of the appropriate
 *	    line to 0
 *	(7) send a hangup to process 1 to poke init to disable getty
 *	(8) post uid name in capitals in _PATH_UTMP to let world know device
 *	    has been grabbed
 *	(9) make sure that DTR is on
 *
 *   enable (i.e.) restore for dialin
 *	(1) check input arguments
 *	(2) look in _PATH_UTMP to check that the line is not in use by another
 *	(3) make sure modem control on terminal is disabled
 *	(4) turn off DTR to make sure line is hung up
 *	(5) condition line: clear exclusive use and set hangup on close modes
 *	(6) turn on modem control
 *	(7) edit _PATH_TTYS, changing the first character of the appropriate
 *	    line to 1
 *	(8) send a hangup to process 1 to poke init to enable getty
 *	(9) clear uid name for _PATH_UTMP
 */

/* #define SENSECARRIER */

#include "uucp.h"
#ifdef DIALINOUT
#include <sys/buf.h>
#include <signal.h>
#include <sys/conf.h>
#ifdef vax
#ifdef BSD4_2
#include <vaxuba/ubavar.h>
#else
#include <sys/ubavar.h>
#endif
#endif /* vax */
#include <sys/stat.h>
#include <nlist.h>
#include <sgtty.h>
#include <utmp.h>
#include <pwd.h>
#include <stdio.h>
#include <sys/file.h>
#include "pathnames.h"

#define NDZLINE	8	/* lines/dz */
#define NDHLINE	16	/* lines/dh */
#define NDMFLINE 8	/* lines/dmf */

#define DZ11	1
#define DH11	2
#define DMF	3

#define NLVALUE(val)	(nl[val].n_value)

struct nlist nl[] = {
#define CDEVSW	0
	{ "_cdevsw" },

#define DZOPEN	1
	{ "_dzopen" },
#define DZINFO	2
	{ "_dzinfo" },
#define NDZ11	3
	{ "_dz_cnt" },
#define DZSCAR	4
	{ "_dzsoftCAR" },

#define DHOPEN	5
	{ "_dhopen" },
#define DHINFO	6
	{ "_dhinfo" },
#define NDH11	7
	{ "_ndh11" },
#define DHSCAR	8
	{ "_dhsoftCAR" },

#define DMFOPEN	9
	{ "_dmfopen" },
#define DMFINFO	10
	{ "_dmfinfo" },
#define NDMF	11
	{ "_ndmf" },
#define DMFSCAR	12
	{ "_dmfsoftCAR" },

	{ "\0" }
};

#define ENABLE	1
#define DISABLE	0

char Etcttys[] = _PATH_TTYS;
#ifdef BSD4_3
FILE *ttysfile, *nttysfile;
char NEtcttys[] = _PATH_NEWTTYS;
extern long ftell();
#endif BSD4_3
char Devhome[] = _PATH_DEV;

char usage[] = "Usage: acucntrl {dis|en}able ttydX\n";

struct utmp utmp;
char resettty, resetmodem;
int etcutmp;
off_t utmploc;
off_t ttyslnbeg;
extern int errno;
extern char *sys_errlist[];
off_t lseek();

#define NAMSIZ	sizeof(utmp.ut_name)
#define	LINSIZ	sizeof(utmp.ut_line)

main(argc, argv)
int argc; char *argv[];
{
	register char *p;
	register int i;
	char uname[NAMSIZ], Uname[NAMSIZ];
	int enable ;
	char *device;
	int devfile;
	int uid, gid;
	struct passwd *getpwuid();
	char *rindex();

	/* check input arguments */
	if (argc!=3 && argc != 4) {
		fprintf(stderr, usage);
		exit(1);
	}

	/* interpret command type */
	if (prefix(argv[1], "disable")  || strcmp(argv[1], "dialout")==0)
		enable = 0;
	else if (prefix(argv[1], "enable")  || strcmp(argv[1], "dialin")==0)
		enable = 1;
	else {
		fprintf(stderr, usage);
		exit(1);
	}

	device = rindex(argv[2], '/');
	device = (device == NULL) ? argv[2]: device+1;

	opnttys(device);

#ifdef vax
	/* Get nlist info */
	nlist(_PATH_UNIX, nl);
#endif vax

	/* Chdir to /dev */
	if(chdir(Devhome) < 0) {
		fprintf(stderr, "Cannot chdir to %s: %s\r\n",
			Devhome, sys_errlist[errno]);
		exit(1);
	}

	/* Get uid information */
	uid = getuid();
	gid = getgid();

	p = getpwuid(uid)->pw_name;
	if (p==NULL) {
		fprintf(stderr, "cannot get uid name\n");
		exit(1);
	}

	if (strcmp(p, "uucp") == 0 && argc == 4)
		p = argv[3];

	/*  to upper case */
	i = 0;
	do {
		uname[i] = *p;
		Uname[i++] = (*p>='a' && *p<='z') ? (*p - ('a'-'A')) : *p;
	} while (*p++ && i<NAMSIZ);

	/* check to see if line is being used */
	if( (etcutmp = open(_PATH_UTMP, 2)) < 0) {
		fprintf(stderr, "On open %s open: %s\n",
			_PATH_UTMP, sys_errlist[errno]);
		exit(1);
	}

	(void)lseek(etcutmp, utmploc, 0);

	i = read(etcutmp, (char *)&utmp, sizeof(struct utmp));

	if(
		i == sizeof(struct utmp) &&
		utmp.ut_line[0] != '\0'  &&
		utmp.ut_name[0] != '\0'  &&
		(
			!upcase(utmp.ut_name, NAMSIZ) ||
			(
				uid != 0 &&
				strncmp(utmp.ut_name, Uname, NAMSIZ) != 0
			)
		)
	) {
		fprintf(stderr, "%s in use by %s\n", device, utmp.ut_name);
		exit(2);
	}

#ifndef sequent
	/* Disable modem control */
	if (setmodem(device, DISABLE) < 0) {
		fprintf(stderr, "Unable to disable modem control\n");
		exit(1);
	}
#endif !sequent

	if (enable) {
#ifdef sequent
		if (setmodem(device, ENABLE) < 0) {
			fprintf(stderr, "Cannot Enable modem control\n");
			(void)setmodem(device, i);
			exit(1);
		}
#endif sequent
#ifndef sequent
		if((devfile = open(device, 1)) < 0) {
			fprintf(stderr, "On open of %s: %s\n",
				device, sys_errlist[errno]);
			(void)setmodem(device, resetmodem);
			exit(1);
		}
		/* Try one last time to hang up */
		if (ioctl(devfile, (int)TIOCCDTR, (char *)0) < 0)
			fprintf(stderr, "On TIOCCDTR ioctl: %s\n",
				sys_errlist[errno]);

		if (ioctl(devfile, (int)TIOCNXCL, (char *)0) < 0)
			fprintf(stderr,
			    "Cannot clear Exclusive Use on %s: %s\n",
				device, sys_errlist[errno]);

		if (ioctl(devfile, (int)TIOCHPCL, (char *)0) < 0)
			fprintf(stderr,
			    "Cannot set hangup on close on %s: %s\n",
				device, sys_errlist[errno]);

#endif !sequent
		i = resetmodem;

#ifndef sequent
		if (setmodem(device, ENABLE) < 0) {
			fprintf(stderr, "Cannot Enable modem control\n");
			(void)setmodem(device, i);
			exit(1);
		}
#endif sequent
		resetmodem=i;

		if (settys(ENABLE)) {
			fprintf(stderr, "%s already enabled\n", device);
		} else {
			pokeinit(device, Uname, enable);
		}
		post(device, "");

	} else {
#if defined(TIOCMGET) && defined(SENSECARRIER)
		if (uid!=0) {
			int linestat = 0;

			/* check for presence of carrier */
			sleep(2); /* need time after modem control turnoff */

			if((devfile = open(device, 1)) < 0) {
				fprintf(stderr, "On open of %s: %s\n",
					device, sys_errlist[errno]);
				(void)setmodem(device, resetmodem);
				exit(1);
			}

			(void)ioctl(devfile, TIOCMGET, &linestat);

			if (linestat&TIOCM_CAR) {
				fprintf(stderr, "%s is in use (Carrier On)\n",
					device);
				(void)setmodem(device, resetmodem);
				exit(2);
			}
			(void)close(devfile);
		}
#endif TIOCMGET
		/* chown device */
		if(chown(device, uid, gid) < 0)
			fprintf(stderr, "Cannot chown %s: %s\n",
				device, sys_errlist[errno]);


		/* poke init */
		if(settys(DISABLE)) {
			fprintf(stderr, "%s already disabled\n", device);
		} else {
			pokeinit(device, Uname, enable);
		}
		post(device, Uname);
#ifdef sequent
	/* Disable modem control */
	if (setmodem(device, DISABLE) < 0) {
		fprintf(stderr, "Unable to disable modem control\n");
		exit(1);
	}
#endif sequent
		if((devfile = open(device, O_RDWR|O_NDELAY)) < 0) {
			fprintf(stderr, "On %s open: %s\n",
				device, sys_errlist[errno]);
		} else {
			if(ioctl(devfile, (int)TIOCSDTR, (char *)0) < 0)
				fprintf(stderr,
				    "Cannot set DTR on %s: %s\n",
					device, sys_errlist[errno]);
		}
	}

	exit(0);
}

/* return true if no lower case */
upcase(str, len)
register char *str;
register int len;
{
	for (; *str, --len >= 0 ; str++)
		if (*str>='a' && *str<='z')
			return(0);
	return(1);
}

/* Post name to public */
post(device, name)
char *device, *name;
{
	(void)time((time_t *)&utmp.ut_time);
	strncpy(utmp.ut_line, device, LINSIZ);
	strncpy(utmp.ut_name, name,  NAMSIZ);
	if (lseek(etcutmp, utmploc, 0) < 0)
		fprintf(stderr, "on lseek in %s: %s",
			_PATH_UTMP, sys_errlist[errno]);
	if (write(etcutmp, (char *)&utmp, sizeof(utmp)) < 0)
		fprintf(stderr, "on write in %s: %s",
			_PATH_UTMP, sys_errlist[errno]);
}
	
/* poke process 1 and wait for it to do its thing */
pokeinit(device, uname, enable)
char *uname, *device; int enable;
{
	struct utmp utmp;
	register int i;

	post(device, uname);

	/* poke init */
	if (kill(1, SIGHUP)) {
		fprintf(stderr,
		    "Cannot send hangup to init process: %s\n",
			sys_errlist[errno]);
		(void)settys(resettty);
		(void)setmodem(device, resetmodem);
		exit(1);
	}

	if (enable)
		return;

	/* wait till init has responded, clearing the utmp entry */
	i = 100;
	do {
		sleep(1);
		if (lseek(etcutmp, utmploc, 0) < 0)
			fprintf(stderr, "On lseek in %s: %s",
				_PATH_UTMP, sys_errlist[errno]);
		if (read(etcutmp, (char *)&utmp, sizeof utmp) < 0)
			fprintf(stderr, "On read from %s: %s",
				_PATH_UTMP, sys_errlist[errno]);
	} while (utmp.ut_name[0] != '\0' && --i > 0);
}

#ifdef BSD4_3
/* identify terminal line in ttys */
opnttys(device)
char *device;
{
	register int  ndevice; 
	register char *p;
	char *index();
	char linebuf[BUFSIZ];

	ttysfile = NULL;
	do {
		if (ttysfile != NULL) {
			fclose(ttysfile);
			sleep(5);
		}
		ttysfile = fopen(Etcttys, "r");
		if(ttysfile == NULL) {
			fprintf(stderr, "Cannot open %s: %s\n", Etcttys,
				sys_errlist[errno]);
			exit(1);
		}
	} while (flock(fileno(ttysfile), LOCK_NB|LOCK_EX) < 0);
	nttysfile = fopen(NEtcttys, "w");
	if(nttysfile == NULL) {
		fprintf(stderr, "Cannot open %s: %s\n", Etcttys,
			sys_errlist[errno]);
		exit(1);
	}

	ndevice = strlen(device);
#ifndef BRL4_2
	utmploc = sizeof(utmp);
#else BRL4_2
	utmploc = 0;
#endif BRL4_2

	while(fgets(linebuf, sizeof(linebuf) - 1, ttysfile) != NULL) {
		if(strncmp(device, linebuf, ndevice) == 0)
			return;
		ttyslnbeg += strlen(linebuf);
		if (linebuf[0] != '#' && linebuf[0] != '\0')
			utmploc += sizeof(utmp);
		if (fputs(linebuf, nttysfile) == NULL) {
			fprintf(stderr, "On %s write: %s\n",
				Etcttys, sys_errlist[errno]);
			exit(1);
		}
		
	}
	fprintf(stderr, "%s not found in %s\n", device, Etcttys);
	exit(1);
}

/* modify appropriate line in _PATH_TTYS to turn on/off the device */
settys(enable)
int enable;
{
	register char *cp, *cp2;
	char lbuf[BUFSIZ];
	int i;
	char c1, c2;

	(void) fseek(ttysfile, ttyslnbeg, 0);
	if(fgets(lbuf, BUFSIZ, ttysfile) == NULL) {
		fprintf(stderr, "On %s read: %s\n",
			Etcttys, sys_errlist[errno]);
		exit(1);
	}
	/* format is now */
	/* ttyd0 std.100 dialup on secure # comment */
	/* except, 2nd item may have embedded spaces inside quotes, Hubert */
	cp = lbuf;
	for (i=0;*cp && i<3;i++) {
		if (*cp == '"') {
			cp++;
			while (*cp && *cp != '"')
				cp++;
			if (*cp != '\0')
				cp++;
		}else {
			while (*cp && *cp != ' ' && *cp != '\t')
				cp++;
		}
		while (*cp && (*cp == ' ' || *cp == '\t'))
			cp++;
	}
	if (*cp == '\0') {
		fprintf(stderr,"Badly formatted line in %s:\n%s",
		    _PATH_TTYS, lbuf);
		exit(1);
	}
	c1 = *--cp;
	*cp++ = '\0';
	cp2 = cp;
	while (*cp && *cp != ' ' && *cp != '\t' && *cp != '\n')
		cp++;
	if (*cp == '\0') {
		fprintf(stderr,"Badly formatted line in %s:\n%s",
		    _PATH_TTYS, lbuf);
		exit(1);
	}
	c2 = *cp;
	*cp++ = '\0';
	while (*cp && (*cp == ' ' || *cp == '\t'))
		cp++;
	resettty = strcmp("on", cp2) != 0;
	fprintf(nttysfile,"%s%c%s%c%s", lbuf, c1, enable ? "on" : "off", c2, cp);
	if (ferror(nttysfile)) {
		fprintf(stderr, "On %s fprintf: %s\n",
			NEtcttys, sys_errlist[errno]);
		exit(1);
	}
	while(fgets(lbuf, sizeof(lbuf) - 1, ttysfile) != NULL) {
		if (fputs(lbuf, nttysfile) == NULL) {
			fprintf(stderr, "On %s write: %s\n",
				NEtcttys, sys_errlist[errno]);
			exit(1);
		}
	}
		
	if (enable^resettty)
		(void) unlink(NEtcttys);
	else {
		struct stat statb;
		if (stat(Etcttys, &statb) == 0) {
			fchmod(fileno(nttysfile) ,statb.st_mode);
			fchown(fileno(nttysfile), statb.st_uid, statb.st_gid);
		}
		(void) rename(NEtcttys, Etcttys);
	}
	(void) fclose(nttysfile);
	(void) fclose(ttysfile);
	return enable^resettty;
}

#else !BSD4_3

/* identify terminal line in ttys */
opnttys(device)
char *device;
{
	register FILE *ttysfile;
	register int  ndevice, lnsiz; 
	register char *p;
	char *index();
	char linebuf[BUFSIZ];

	ttysfile = fopen(Etcttys, "r");
	if(ttysfile == NULL) {
		fprintf(stderr, "Cannot open %s: %s\n", Etcttys,
			sys_errlist[errno]);
		exit(1);
	}

	ndevice = strlen(device);
	ttyslnbeg = 0;
	utmploc = 0;

	while(fgets(linebuf, sizeof(linebuf) - 1, ttysfile) != NULL) {
		lnsiz = strlen(linebuf);
		if ((p = index(linebuf, '\n')) != NULL)
			*p = '\0';
		if(strncmp(device, &linebuf[2], ndevice) == 0) {
			(void)fclose(ttysfile);
#ifdef sequent
			/* Why is the sequent off by one? */
			utmploc += sizeof(utmp);
#endif sequent
			return;
		}
		ttyslnbeg += lnsiz;
		utmploc += sizeof(utmp);
	}
	fprintf(stderr, "%s not found in %s\n", device, Etcttys);
	exit(1);
}

/* modify appropriate line in _PATH_TTYS to turn on/off the device */
settys(enable)
int enable;
{
	int ittysfil;
	char out, in;

	ittysfil = open(Etcttys, 2);
	if(ittysfil < 0) {
		fprintf(stderr, "Cannot open %s for output: %s\n",
			Etcttys, sys_errlist[errno]);
		exit(1);
	}
	(void)lseek(ittysfil, ttyslnbeg, 0);
	if(read(ittysfil, &in, 1)<0) {
		fprintf(stderr, "On %s write: %s\n",
			Etcttys, sys_errlist[errno]);
		exit(1);
	}
	resettty = (in == '1');
	out = enable ? '1' : '0';
	(void)lseek(ittysfil, ttyslnbeg, 0);
	if(write(ittysfil, &out, 1)<0) {
		fprintf(stderr, "On %s write: %s\n",
			Etcttys, sys_errlist[errno]);
		exit(1);
	}
	(void)close(ittysfil);
	return(in==out);
}
#endif !BSD4_3

#ifdef sequent
setmodem(ttyline, enable)
char *ttyline; int enable;
{
	char *sysbuf[BUFSIZ];
	sprintf(sysbuf,"/etc/ttyconfig /dev/%s -special %s", ttyline,
		enable ? "-carrier" : "-nocarrier");
	system(sysbuf);
}
#endif /* sequent */
#ifdef vax
/*
 * Excerpted from (June 8, 1983 W.Sebok)
 * > ttymodem.c - enable/disable modem control for tty lines.
 * >
 * > Knows about DZ11s and DH11/DM11s.
 * > 23.3.83 - TS
 * > modified to know about DMF's  (hasn't been tested) Nov 8, 1984 - WLS
 */


setmodem(ttyline, enable)
char *ttyline; int enable;
{
	dev_t dev;
	int kmem;
	int unit, line, nlines, addr, tflags;
	int devtype=0;
	char cflags; short sflags;
#ifdef BSD4_2
	int flags;
#else
	short flags;
#endif
	struct uba_device *ubinfo;
	struct stat statb;
	struct cdevsw cdevsw;

	if(nl[CDEVSW].n_type == 0) {
		fprintf(stderr, "No namelist.\n");
		return(-1);
	}

	if((kmem = open(_PATH_KMEM, 2)) < 0) {
		fprintf(stderr, "%s open: %s\n", _PATH_KMEM,
		    sys_errlist[errno]);
		return(-1);
	}

	if(stat(ttyline, &statb) < 0) {
		fprintf(stderr, "%s stat: %s\n", ttyline, sys_errlist[errno]);
		return(-1);
	}

	if((statb.st_mode&S_IFMT) != S_IFCHR) {
		fprintf(stderr, "%s is not a character device.\n",ttyline);
		return(-1);
	}

	dev = statb.st_rdev;
	(void)lseek(kmem,
		(off_t) &(((struct cdevsw *)NLVALUE(CDEVSW))[major(dev)]),0);
	(void)read(kmem, (char *) &cdevsw, sizeof cdevsw);

	if((int)(cdevsw.d_open) == NLVALUE(DZOPEN)) {
		devtype = DZ11;
		unit = minor(dev) / NDZLINE;
		line = minor(dev) % NDZLINE;
		addr = (int) &(((int *)NLVALUE(DZINFO))[unit]);
		(void)lseek(kmem, (off_t) NLVALUE(NDZ11), 0);
	} else if((int)(cdevsw.d_open) == NLVALUE(DHOPEN)) {
		devtype = DH11;
		unit = minor(dev) / NDHLINE;
		line = minor(dev) % NDHLINE;
		addr = (int) &(((int *)NLVALUE(DHINFO))[unit]);
		(void)lseek(kmem, (off_t) NLVALUE(NDH11), 0);
	} else if((int)(cdevsw.d_open) == NLVALUE(DMFOPEN)) {
		devtype = DMF;
		unit = minor(dev) / NDMFLINE;
		line = minor(dev) % NDMFLINE;
		addr = (int) &(((int *)NLVALUE(DMFINFO))[unit]);
		(void)lseek(kmem, (off_t) NLVALUE(NDMF), 0);
	} else {
		fprintf(stderr, "Device %s (%d/%d) unknown.\n", ttyline,
		    major(dev), minor(dev));
		return(-1);
	}

	(void)read(kmem, (char *) &nlines, sizeof nlines);
	if(minor(dev) >= nlines) {
		fprintf(stderr, "Sub-device %d does not exist (only %d).\n",
		    minor(dev), nlines);
		return(-1);
	}

	(void)lseek(kmem, (off_t)addr, 0);
	(void)read(kmem, (char *) &ubinfo, sizeof ubinfo);
	(void)lseek(kmem, (off_t) &(ubinfo->ui_flags), 0);
	(void)read(kmem, (char *) &flags, sizeof flags);

	tflags = 1<<line;
	resetmodem = ((flags&tflags) == 0);
	flags = enable ? (flags & ~tflags) : (flags | tflags);
	(void)lseek(kmem, (off_t) &(ubinfo->ui_flags), 0);
	(void)write(kmem, (char *) &flags, sizeof flags);
	switch(devtype) {
		case DZ11:
			if((addr = NLVALUE(DZSCAR)) == 0) {
				fprintf(stderr, "No dzsoftCAR.\n");
				return(-1);
			}
			cflags = flags;
			(void)lseek(kmem, (off_t) &(((char *)addr)[unit]), 0);
			(void)write(kmem, (char *) &cflags, sizeof cflags);
			break;
		case DH11:
			if((addr = NLVALUE(DHSCAR)) == 0) {
				fprintf(stderr, "No dhsoftCAR.\n");
				return(-1);
			}
			sflags = flags;
			(void)lseek(kmem, (off_t) &(((short *)addr)[unit]), 0);
			(void)write(kmem, (char *) &sflags, sizeof sflags);
			break;
		case DMF:
			if((addr = NLVALUE(DMFSCAR)) == 0) {
				fprintf(stderr, "No dmfsoftCAR.\n");
				return(-1);
			}
			cflags = flags;
			(void)lseek(kmem, (off_t) &(((char *)addr)[unit]), 0);
			(void)write(kmem, (char *) &cflags, sizeof cflags);
			break;
		default:
			fprintf(stderr, "Unknown device type\n");
			return(-1);
	}
	return(0);
}
#endif /* vax */

prefix(s1, s2)
	register char *s1, *s2;
{
	register char c;

	while ((c = *s1++) == *s2++)
		if (c == '\0')
			return (1);
	return (c == '\0');
}
#else	/* !DIALINOUT */
main()
{
	fprintf(stderr,"acucntrl is not supported on this system\n");
}
#endif /* !DIALINOUT */
