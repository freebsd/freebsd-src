/*
 * Copyright (c) 1980, 1986, 1989 The Regents of the University of California.
 * All rights reserved.
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
char copyright[] =
"@(#) Copyright (c) 1980, 1986, 1989 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)savecore.c	5.26 (Berkeley) 4/8/91";
#endif /* not lint */

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/file.h>
#include <sys/syslog.h>
#include <dirent.h>
#include <stdio.h>
#include <nlist.h>
#include <paths.h>

#define	DAY	(60L*60L*24L)
#define	LEEWAY	(3*DAY)

#define eq(a,b) (!strcmp(a,b))
#ifdef vax
#define ok(number) ((number)&0x7fffffff)
#else
#ifdef tahoe
#define ok(number) ((number)&~0xc0000000)
#else
#ifdef i386
#define ok(number) ((number)&~0xfe000000)
#else
#define ok(number) (number)
#endif
#endif
#endif

struct nlist current_nl[] = {	/* namelist for currently running system */
#define X_DUMPDEV	0
	{ "_dumpdev" },
#define X_DUMPLO	1
	{ "_dumplo" },
#define X_TIME		2
	{ "_time" },
#define	X_DUMPSIZE	3
	{ "_dumpsize" },
#define X_VERSION	4
	{ "_version" },
#define X_PANICSTR	5
	{ "_panicstr" },
#define	X_DUMPMAG	6
	{ "_dumpmag" },
	{ "" },
};

struct nlist dump_nl[] = {	/* name list for dumped system */
	{ "_dumpdev" },		/* entries MUST be the same as */
	{ "_dumplo" },		/*	those in current_nl[]  */
	{ "_time" },
	{ "_dumpsize" },
	{ "_version" },
	{ "_panicstr" },
	{ "_dumpmag" },
	{ "" },
};

char	*system;
char	*dirname;			/* directory to save dumps in */
char	*ddname;			/* name of dump device */
int	dumpfd;				/* read/write descriptor on block dev */
char	*find_dev();
dev_t	dumpdev;			/* dump device */
time_t	dumptime;			/* time the dump was taken */
int	dumplo;				/* where dump starts on dumpdev */
int	dumpsize;			/* amount of memory dumped */
int	dumpmag;			/* magic number in dump */
time_t	now;				/* current date */
char	*path();
char	*malloc();
char	*ctime();
char	vers[80];
char	core_vers[80];
char	panic_mesg[80];
int	panicstr;
off_t	lseek();
off_t	Lseek();
int	verbose;
int	force;
int	clear;
extern	int errno;

main(argc, argv)
	char **argv;
	int argc;
{
	extern char *optarg;
	extern int optind;
	int ch;
	char *cp;

	while ((ch = getopt(argc, argv, "cdfv")) != EOF)
		switch(ch) {
		case 'c':
			clear = 1;
			break;
		case 'd':		/* not documented */
		case 'v':
			verbose = 1;
			break;
		case 'f':
			force = 1;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	/* This is wrong, but I want "savecore -c" to work. */
	if (!clear) {
		if (argc != 1 && argc != 2)
			usage();
		dirname = argv[0];
	}
	if (argc == 2)
		system = argv[1];

	openlog("savecore", LOG_ODELAY, LOG_AUTH);

	read_kmem();
	if (!dump_exists()) {
/*		(void)fprintf(stderr, "savecore: no core dump\n");*/
		if (!force)
			exit(0);
	}
	if (clear) {
		clear_dump();
		exit(0);
	}
	(void) time(&now);
	check_kmem();
	if (panicstr)
		log(LOG_CRIT, "reboot after panic: %s\n", panic_mesg);
	else
		syslog(LOG_CRIT, "reboot\n");

	if (access(dirname, W_OK) < 0) {
		Perror(LOG_ERR, "%s: %m\n", dirname);
		exit(1);
	}
	if ((!get_crashtime() || !check_space()) && !force)
		exit(1);
	save_core();
	clear_dump();
	exit(0);
}

dump_exists()
{
	int word;

	Lseek(dumpfd, (off_t)(dumplo + ok(dump_nl[X_DUMPMAG].n_value)), L_SET);
	Read(dumpfd, (char *)&word, sizeof (word));
	if (verbose && word != dumpmag)
		printf("magic number mismatch: %x != %x\n", word, dumpmag);
	return (word == dumpmag);
}

clear_dump()
{
	int zero = 0;

	Lseek(dumpfd, (off_t)(dumplo + ok(dump_nl[X_DUMPMAG].n_value)), L_SET);
	Write(dumpfd, (char *)&zero, sizeof (zero));
}

char *
find_dev(dev, type)
	register dev_t dev;
	register int type;
{
	register DIR *dfd = opendir(_PATH_DEV);
	struct dirent *dir;
	struct stat statb;
	static char devname[MAXPATHLEN + 1];
	char *dp;

	strcpy(devname, _PATH_DEV);
	while ((dir = readdir(dfd))) {
		strcpy(devname + sizeof(_PATH_DEV) - 1, dir->d_name);
		if (stat(devname, &statb)) {
			perror(devname);
			continue;
		}
		if ((statb.st_mode&S_IFMT) != type)
			continue;
		if (dev == statb.st_rdev) {
			closedir(dfd);
			dp = malloc(strlen(devname)+1);
			strcpy(dp, devname);
			return (dp);
		}
	}
	closedir(dfd);
	log(LOG_ERR, "Can't find device %d/%d\n", major(dev), minor(dev));
	exit(1);
	/*NOTREACHED*/
}

char *
rawname(s)
	char *s;
{
	static char name[MAXPATHLEN];
	char *sl, *rindex();

	if ((sl = rindex(s, '/')) == NULL || sl[1] == '0') {
		log(LOG_ERR, "can't make raw dump device name from %s?\n", s);
		return (s);
	}
	sprintf(name, "%.*s/r%s", sl - s, s, sl + 1);
	return (name);
}

int	cursyms[] =
    { X_DUMPDEV, X_DUMPLO, X_VERSION, X_DUMPMAG, -1 };
int	dumpsyms[] =
    { X_TIME, X_DUMPSIZE, X_VERSION, X_PANICSTR, X_DUMPMAG, -1 };
read_kmem()
{
	register char *cp;
	FILE *fp;
	char *dump_sys;
	int kmem, i;
	
	dump_sys = system ? system : _PATH_UNIX;
	nlist(_PATH_UNIX, current_nl);
	nlist(dump_sys, dump_nl);
	/*
	 * Some names we need for the currently running system,
	 * others for the system that was running when the dump was made.
	 * The values obtained from the current system are used
	 * to look for things in /dev/kmem that cannot be found
	 * in the dump_sys namelist, but are presumed to be the same
	 * (since the disk partitions are probably the same!)
	 */
	for (i = 0; cursyms[i] != -1; i++)
		if (current_nl[cursyms[i]].n_value == 0) {
			log(LOG_ERR, "%s: %s not in namelist\n", _PATH_UNIX,
			    current_nl[cursyms[i]].n_name);
			exit(1);
		}
	for (i = 0; dumpsyms[i] != -1; i++)
		if (dump_nl[dumpsyms[i]].n_value == 0) {
			log(LOG_ERR, "%s: %s not in namelist\n", dump_sys,
			    dump_nl[dumpsyms[i]].n_name);
			exit(1);
		}
	kmem = Open(_PATH_KMEM, O_RDONLY);
	Lseek(kmem, (long)current_nl[X_DUMPDEV].n_value, L_SET);
	Read(kmem, (char *)&dumpdev, sizeof (dumpdev));
	Lseek(kmem, (long)current_nl[X_DUMPLO].n_value, L_SET);
	Read(kmem, (char *)&dumplo, sizeof (dumplo));
	if (verbose)
		printf("dumplo = %d (%d * %d)\n", dumplo, dumplo/DEV_BSIZE,
		    DEV_BSIZE);
	Lseek(kmem, (long)current_nl[X_DUMPMAG].n_value, L_SET);
	Read(kmem, (char *)&dumpmag, sizeof (dumpmag));
	dumplo *= DEV_BSIZE;
	ddname = find_dev(dumpdev, S_IFBLK);
	dumpfd = Open(ddname, O_RDWR);
	fp = fdopen(kmem, "r");
	if (fp == NULL) {
		log(LOG_ERR, "Couldn't fdopen kmem\n");
		exit(1);
	}
	if (system)
		return;
	fseek(fp, (long)current_nl[X_VERSION].n_value, L_SET);
	fgets(vers, sizeof (vers), fp);
	fclose(fp);
}

check_kmem()
{
	FILE *fp;
	register char *cp;

	fp = fdopen(dumpfd, "r");
	if (fp == NULL) {
		log(LOG_ERR, "Can't fdopen dumpfd\n");
		exit(1);
	}

	fseek(fp, (off_t)(dumplo+ok(dump_nl[X_VERSION].n_value)), L_SET);
	fgets(core_vers, sizeof (core_vers), fp);
	if (!eq(vers, core_vers) && system == 0) {
		log(LOG_WARNING, "Warning: %s version mismatch:\n", _PATH_UNIX);
		log(LOG_WARNING, "\t%s\n", vers);
		log(LOG_WARNING, "and\t%s\n", core_vers);
	}

	fseek(fp, (off_t)(dumplo + ok(dump_nl[X_PANICSTR].n_value)), L_SET);
	fread((char *)&panicstr, sizeof (panicstr), 1, fp);
	if (panicstr) {
		fseek(fp, dumplo + ok(panicstr), L_SET);
		cp = panic_mesg;
		do
			*cp = getc(fp);
		while (*cp++ && cp < &panic_mesg[sizeof(panic_mesg)]);
	}
	/* don't fclose(fp); we want the file descriptor */
}

get_crashtime()
{
	time_t clobber = (time_t)0;

	Lseek(dumpfd, (off_t)(dumplo + ok(dump_nl[X_TIME].n_value)), L_SET);
	Read(dumpfd, (char *)&dumptime, sizeof dumptime);
	if (dumptime == 0) {
		if (verbose)
			printf("Dump time is zero.\n");
		return (0);
	}
	printf("System went down at %s", ctime(&dumptime));
	if (dumptime < now - LEEWAY || dumptime > now + LEEWAY) {
		printf("dump time is unreasonable\n");
		return (0);
	}
	return (1);
}

char *
path(file)
	char *file;
{
	register char *cp = malloc(strlen(file) + strlen(dirname) + 2);

	(void) strcpy(cp, dirname);
	(void) strcat(cp, "/");
	(void) strcat(cp, file);
	return (cp);
}

check_space()
{
	long minfree, spacefree;
	struct statfs fsbuf;

	if (statfs(dirname, &fsbuf) < 0) {
		Perror(LOG_ERR, "%s: %m\n", dirname);
		exit(1);
	}
 	spacefree = fsbuf.f_bavail * fsbuf.f_fsize / 1024;
	minfree = read_number("minfree");
 	if (minfree > 0 && spacefree - dumpsize < minfree) {
		log(LOG_WARNING, "Dump omitted, not enough space on device\n");
		return (0);
	}
	if (spacefree - dumpsize < minfree)
		log(LOG_WARNING,
		    "Dump performed, but free space threshold crossed\n");
	return (1);
}

read_number(fn)
	char *fn;
{
	char lin[80];
	register FILE *fp;

	fp = fopen(path(fn), "r");
	if (fp == NULL)
		return (0);
	if (fgets(lin, 80, fp) == NULL) {
		fclose(fp);
		return (0);
	}
	fclose(fp);
	return (atoi(lin));
}

/*#define	BUFSIZE		(256*1024)		/* 1/4 Mb */
#define	BUFSIZE		(8*1024)

save_core()
{
	register int n;
	register char *cp;
	register int ifd, ofd, bounds;
	int ret;
	char *bfile;
	register FILE *fp;

	cp = malloc(BUFSIZE);
	if (cp == 0) {
		log(LOG_ERR, "savecore: Can't allocate i/o buffer.\n");
		return;
	}
	bounds = read_number("bounds");
	ifd = Open(system ? system : _PATH_UNIX, O_RDONLY);
	(void)sprintf(cp, "system.%d", bounds);
	ofd = Create(path(cp), 0644);
	while((n = Read(ifd, cp, BUFSIZE)) > 0)
		Write(ofd, cp, n);
	close(ifd);
	close(ofd);
	if ((ifd = open(rawname(ddname), O_RDONLY)) == -1) {
		log(LOG_WARNING, "Can't open %s (%m); using block device",
			rawname(ddname));
		ifd = dumpfd;
	}
	Lseek(dumpfd, (off_t)(dumplo + ok(dump_nl[X_DUMPSIZE].n_value)), L_SET);
	Read(dumpfd, (char *)&dumpsize, sizeof (dumpsize));
	(void)sprintf(cp, "ram.%d", bounds);
	ofd = Create(path(cp), 0644);
	Lseek(ifd, (off_t)dumplo, L_SET);
	dumpsize *= NBPG;
	log(LOG_NOTICE, "Saving %d bytes of image in ram.%d\n",
	    dumpsize, bounds);
	while (dumpsize > 0) {
		n = read(ifd, cp,
		    dumpsize > BUFSIZE ? BUFSIZE : dumpsize);
		if (n <= 0) {
			if (n == 0)
				log(LOG_WARNING,
				    "WARNING: EOF on dump device; %s\n",
				    "ram file may be incomplete");
			else
				Perror(LOG_ERR, "read from dumpdev: %m",
				    "read");
			break;
		}
		if ((ret = write(ofd, cp, n)) < n) {
			if (ret < 0)
				Perror(LOG_ERR, "write: %m", "write");
			else
				log(LOG_ERR, "short write: wrote %d of %d\n",
				    ret, n);
			log(LOG_WARNING, "WARNING: ram file may be incomplete\n");
			break;
		}
		dumpsize -= n;
	}
	close(ifd);
	close(ofd);
	bfile = path("bounds");
	fp = fopen(bfile, "w");
	if (fp) {
		fprintf(fp, "%d\n", bounds+1);
		fclose(fp);
	} else
		Perror(LOG_ERR, "Can't create bounds file %s: %m", bfile);
	free(cp);
}

/*
 * Versions of std routines that exit on error.
 */
Open(name, rw)
	char *name;
	int rw;
{
	int fd;

	fd = open(name, rw);
	if (fd < 0) {
		Perror(LOG_ERR, "%s: %m", name);
		exit(1);
	}
	return (fd);
}

Read(fd, buff, size)
	int fd, size;
	char *buff;
{
	int ret;

	ret = read(fd, buff, size);
	if (ret < 0) {
		Perror(LOG_ERR, "read: %m", "read");
		exit(1);
	}
	return (ret);
}

off_t
Lseek(fd, off, flag)
	int fd, flag;
	long off;
{
	long ret;

	ret = lseek(fd, off, flag);
	if (ret == -1) {
		Perror(LOG_ERR, "lseek: %m", "lseek");
		exit(1);
	}
	return (ret);
}

Create(file, mode)
	char *file;
	int mode;
{
	register int fd;

	fd = creat(file, mode);
	if (fd < 0) {
		Perror(LOG_ERR, "%s: %m", file);
		exit(1);
	}
	return (fd);
}

Write(fd, buf, size)
	int fd, size;
	char *buf;
{
	int n;

	if ((n = write(fd, buf, size)) < size) {
		if (n < 0)
			Perror(LOG_ERR, "write: %m", "write");
		else
			log(LOG_ERR, "short write: wrote %d of %d\n", n, size);
		exit(1);
	}
}

/* VARARGS2 */
log(level, msg, a1, a2)
	int level;
	char *msg;
{

	fprintf(stderr, msg, a1, a2);
	syslog(level, msg, a1, a2);
}

Perror(level, msg, s)
	int level;
	char *msg, *s;
{
	int oerrno = errno;
	
	perror(s);
	errno = oerrno;
	syslog(level, msg, s);
}

usage()
{
	(void)fprintf(stderr, "usage: savecore [-cfv] dirname [system]\n");
	exit(1);
}
