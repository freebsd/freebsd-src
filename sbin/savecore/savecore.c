/*-
 * Copyright (c) 1986, 1992, 1993
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
static const char copyright[] =
"@(#) Copyright (c) 1986, 1992, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)savecore.c	8.3 (Berkeley) 1/2/94";
#endif
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/syslog.h>
#include <sys/sysctl.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

#include <dirent.h>
#include <fcntl.h>
#include <nlist.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern FILE *zopen(const char *fname, const char *mode);

#ifdef __alpha__
#define ok(number) ALPHA_K0SEG_TO_PHYS(number)
#endif

#ifdef __i386__
#define ok(number) ((number) - kernbase)
#endif

struct nlist current_nl[] = {	/* Namelist for currently running system. */
#define X_DUMPLO	0
	{ "_dumplo" },
#define X_TIME		1
	{ "_time_second" },
#define	X_DUMPSIZE	2
	{ "_dumpsize" },
#define X_VERSION	3
	{ "_version" },
#define X_PANICSTR	4
	{ "_panicstr" },
#define	X_DUMPMAG	5
	{ "_dumpmag" },
#define	X_KERNBASE	6
	{ "_kernbase" },
	{ "" },
};
int cursyms[] = { X_DUMPLO, X_VERSION, X_DUMPMAG, -1 };
int dumpsyms[] = { X_TIME, X_DUMPSIZE, X_VERSION, X_PANICSTR, X_DUMPMAG, -1 };

struct nlist dump_nl[] = {	/* Name list for dumped system. */
	{ "_dumplo" },		/* Entries MUST be the same as */
	{ "_time_second" },	/*	those in current_nl[].  */
	{ "_dumpsize" },
	{ "_version" },
	{ "_panicstr" },
	{ "_dumpmag" },
	{ "_kernbase" },
	{ "" },
};

/* Types match kernel declarations. */
u_long	dumpmag;			/* magic number in dump */

/* Based on kernel variables, but with more convenient types. */
off_t	dumplo;				/* where dump starts on dumpdev */
off_t	dumpsize;			/* amount of memory dumped */

char	*kernel;			/* user-specified kernel */
char	*savedir;			/* directory to save dumps in */
char	ddname[MAXPATHLEN];		/* name of dump device */
dev_t	dumpdev;			/* dump device */
int	dumpfd;				/* read/write descriptor on char dev */
time_t	now;				/* current date */
char	panic_mesg[1024];		/* panic message */
int	panicstr;		        /* flag: dump was caused by panic */
char	vers[1024];			/* version of kernel that crashed */

#ifdef __i386__
u_long	kernbase;			/* offset of kvm to core file */
#endif

int	clear, compress, force, verbose;	/* flags */
int	keep;			/* keep dump on device */

void	 check_kmem __P((void));
int	 check_space __P((void));
void	 clear_dump __P((void));
void	 DumpRead __P((int fd, void *bp, int size, off_t off, int flag));
void	 DumpWrite __P((int fd, void *bp, int size, off_t off, int flag));
int	 dump_exists __P((void));
void     find_dev __P((dev_t));
int	 get_crashtime __P((void));
void	 get_dumpsize __P((void));
void	 kmem_setup __P((void));
void	 log __P((int, char *, ...)) __printf0like(2, 3);
void	 Lseek __P((int, off_t, int));
int	 Open __P((const char *, int rw));
int	 Read __P((int, void *, int));
void	 save_core __P((void));
void	 usage __P((void));
void	 Write __P((int, void *, int));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int ch;

	openlog("savecore", LOG_PERROR, LOG_DAEMON);

	while ((ch = getopt(argc, argv, "cdfkN:vz")) != -1)
		switch(ch) {
		case 'c':
			clear = 1;
			break;
		case 'd':		/* Not documented. */
		case 'v':
			verbose = 1;
			break;
		case 'f':
			force = 1;
			break;
		case 'k':
			keep = 1;
			break;
		case 'N':
			kernel = optarg;
			break;
		case 'z':
			compress = 1;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (!clear) {
		if (argc != 1 && argc != 2)
			usage();
		savedir = argv[0];
	}
	if (argc == 2)
		kernel = argv[1];

	(void)time(&now);
	kmem_setup();

	if (clear) {
		clear_dump();
		exit(0);
	}

	if (!dump_exists() && !force)
		exit(1);

	check_kmem();

	if (panicstr)
		syslog(LOG_ALERT, "reboot after panic: %s", panic_mesg);
	else
		syslog(LOG_ALERT, "reboot");

	get_dumpsize();

	if ((!get_crashtime() || !check_space()) && !force)
		exit(1);

	save_core();

	if (!keep)
		clear_dump();
	
	exit(0);
}

void
kmem_setup()
{
	int kmem, i;
	const char *dump_sys;
	size_t len;
	long kdumplo;		/* block number where dump starts on dumpdev */
	char *p;

	/*
	 * Some names we need for the currently running system, others for
	 * the system that was running when the dump was made.  The values
	 * obtained from the current system are used to look for things in
	 * /dev/kmem that cannot be found in the dump_sys namelist, but are
	 * presumed to be the same (since the disk partitions are probably
	 * the same!)
	 */
	if ((nlist(getbootfile(), current_nl)) == -1)
		syslog(LOG_ERR, "%s: nlist: %m", getbootfile());
	for (i = 0; cursyms[i] != -1; i++)
		if (current_nl[cursyms[i]].n_value == 0) {
			syslog(LOG_ERR, "%s: %s not in namelist",
			    getbootfile(), current_nl[cursyms[i]].n_name);
			exit(1);
		}

	dump_sys = kernel ? kernel : getbootfile();
	if ((nlist(dump_sys, dump_nl)) == -1)
		syslog(LOG_ERR, "%s: nlist: %m", dump_sys);
	for (i = 0; dumpsyms[i] != -1; i++)
		if (dump_nl[dumpsyms[i]].n_value == 0) {
			syslog(LOG_ERR, "%s: %s not in namelist",
			    dump_sys, dump_nl[dumpsyms[i]].n_name);
			exit(1);
		}

#ifdef __i386__
	if (dump_nl[X_KERNBASE].n_value != 0)
		kernbase = dump_nl[X_KERNBASE].n_value;
	else
		kernbase = KERNBASE;
#endif

	len = sizeof dumpdev;
	if (sysctlbyname("kern.dumpdev", &dumpdev, &len, NULL, 0) == -1) {
		syslog(LOG_ERR, "sysctl: kern.dumpdev: %m");
		exit(1);
	}
	if (dumpdev == NODEV) {
		syslog(LOG_WARNING, "no core dump (no dumpdev)");
		exit(1);
	}

	kmem = Open(_PATH_KMEM, O_RDONLY);
	Lseek(kmem, (off_t)current_nl[X_DUMPLO].n_value, L_SET);
	(void)Read(kmem, &kdumplo, sizeof(kdumplo));
	dumplo = (off_t)kdumplo * DEV_BSIZE;
	if (verbose)
		(void)printf("dumplo = %lld (%ld * %d)\n",
		    (long long)dumplo, kdumplo, DEV_BSIZE);
	Lseek(kmem, (off_t)current_nl[X_DUMPMAG].n_value, L_SET);
	(void)Read(kmem, &dumpmag, sizeof(dumpmag));
	find_dev(dumpdev);
	dumpfd = Open(ddname, O_RDWR);
	if (kernel)
		return;

	lseek(kmem, (off_t)current_nl[X_VERSION].n_value, SEEK_SET);
	Read(kmem, vers, sizeof(vers));
	vers[sizeof(vers) - 1] = '\0';
	p = strchr(vers, '\n');
	if (p)
		p[1] = '\0';

	/* Don't fclose(fp), we use kmem later. */
}

void
check_kmem()
{
	char core_vers[1024], *p;

	DumpRead(dumpfd, core_vers, sizeof(core_vers),
	    (off_t)(dumplo + ok(dump_nl[X_VERSION].n_value)), L_SET);
	core_vers[sizeof(core_vers) - 1] = '\0';
	p = strchr(core_vers, '\n');
	if (p)
		p[1] = '\0';
	if (strcmp(vers, core_vers) && kernel == 0)
		syslog(LOG_WARNING,
		    "warning: %s version mismatch:\n\t\"%s\"\nand\t\"%s\"\n",
		    getbootfile(), vers, core_vers);
	DumpRead(dumpfd, &panicstr, sizeof(panicstr),
	    (off_t)(dumplo + ok(dump_nl[X_PANICSTR].n_value)), L_SET);
	if (panicstr) {
		DumpRead(dumpfd, panic_mesg, sizeof(panic_mesg),
		    (off_t)(dumplo + ok(panicstr)), L_SET);
	}
}

/*
 * Clear the magic number in the dump header.
 */
void
clear_dump()
{
	u_long newdumpmag;

	newdumpmag = 0;
	DumpWrite(dumpfd, &newdumpmag, sizeof(newdumpmag),
	    (off_t)(dumplo + ok(dump_nl[X_DUMPMAG].n_value)), L_SET);
	close(dumpfd);
}

/*
 * Check if a dump exists by looking for a magic number in the dump
 * header.
 */
int
dump_exists()
{
	u_long newdumpmag;

	DumpRead(dumpfd, &newdumpmag, sizeof(newdumpmag),
	    (off_t)(dumplo + ok(dump_nl[X_DUMPMAG].n_value)), L_SET);
	if (newdumpmag != dumpmag) {
		if (verbose)
			syslog(LOG_WARNING, "magic number mismatch (%x != %x)",
			    newdumpmag, dumpmag);
		syslog(LOG_WARNING, "no core dump");
		return (0);
	}
	return (1);
}

char buf[1024 * 1024];
#define BLOCKSIZE (1<<12)
#define BLOCKMASK (~(BLOCKSIZE-1))

/*
 * Save the core dump.
 */
void
save_core()
{
	register FILE *fp;
	register int bounds, ifd, nr, nw;
	int hs, he;		/* start and end of hole */
	char path[MAXPATHLEN];
	mode_t oumask;

	/*
	 * Get the current number and update the bounds file.  Do the update
	 * now, because may fail later and don't want to overwrite anything.
	 */
	(void)snprintf(path, sizeof(path), "%s/bounds", savedir);
	if ((fp = fopen(path, "r")) == NULL)
		goto err1;
	if (fgets(buf, sizeof(buf), fp) == NULL) {
		if (ferror(fp))
err1:			syslog(LOG_WARNING, "%s: %m", path);
		bounds = 0;
	} else
		bounds = atoi(buf);
	if (fp != NULL)
		(void)fclose(fp);
	if ((fp = fopen(path, "w")) == NULL)
		syslog(LOG_ERR, "%s: %m", path);
	else {
		(void)fprintf(fp, "%d\n", bounds + 1);
		(void)fclose(fp);
	}

	/* Create the core file. */
	oumask = umask(S_IRWXG|S_IRWXO); /* Restrict access to the core file.*/
	(void)snprintf(path, sizeof(path), "%s/vmcore.%d%s",
	    savedir, bounds, compress ? ".gz" : "");
	if (compress)
		fp = zopen(path, "w");
	else
		fp = fopen(path, "w");
	if (fp == NULL) {
		syslog(LOG_ERR, "%s: %m", path);
		exit(1);
	}
	(void)umask(oumask);

	/* Seek to the start of the core. */
	Lseek(dumpfd, (off_t)dumplo, L_SET);

	/* Copy the core file. */
	syslog(LOG_NOTICE, "writing %score to %s",
	    compress ? "compressed " : "", path);
	for (; dumpsize > 0; dumpsize -= nr) {
		(void)printf("%6dK\r", dumpsize / 1024);
		(void)fflush(stdout);
		nr = read(dumpfd, buf, MIN(dumpsize, sizeof(buf)));
		if (nr <= 0) {
			if (nr == 0)
				syslog(LOG_WARNING,
				    "WARNING: EOF on dump device");
			else
				syslog(LOG_ERR, "%s: %m", ddname);
			goto err2;
		}
		for (nw = 0; nw < nr; nw = he) {
			/* find a contiguous block of zeroes */
			for (hs = nw; hs < nr; hs += BLOCKSIZE) {
				for (he = hs; he < nr && buf[he] == 0; ++he)
					/* nothing */ ;

				/* is the hole long enough to matter? */
				if (he >= hs + BLOCKSIZE)
					break;
			}
			
			/* back down to a block boundary */
			he &= BLOCKMASK;

			/*
			 * 1) Don't go beyond the end of the buffer.
			 * 2) If the end of the buffer is less than
			 *    BLOCKSIZE bytes away, we're at the end
			 *    of the file, so just grab what's left.
			 */
			if (hs + BLOCKSIZE > nr)
				hs = he = nr;
			
			/*
			 * At this point, we have a partial ordering:
			 *     nw <= hs <= he <= nr
			 * If hs > nw, buf[nw..hs] contains non-zero data.
			 * If he > hs, buf[hs..he] is all zeroes.
			 */
			if (hs > nw)
				if (fwrite(buf + nw, hs - nw, 1, fp) != 1)
					break;
			if (he > hs)
				if (fseek(fp, he - hs, SEEK_CUR) == -1)
					break;
		}
		if (nw != nr) {
			syslog(LOG_ERR, "%s: %m", path);
err2:			syslog(LOG_WARNING,
			    "WARNING: vmcore may be incomplete");
			(void)printf("\n");
			exit(1);
		}
	}

	(void)fclose(fp);

	/* Copy the kernel. */
	ifd = Open(kernel ? kernel : getbootfile(), O_RDONLY);
	(void)snprintf(path, sizeof(path), "%s/kernel.%d%s",
	    savedir, bounds, compress ? ".gz" : "");
	if (compress)
		fp = zopen(path, "w");
	else
		fp = fopen(path, "w");
	if (fp == NULL) {
		syslog(LOG_ERR, "%s: %m", path);
		exit(1);
	}
	syslog(LOG_NOTICE, "writing %skernel to %s",
	    compress ? "compressed " : "", path);
	while ((nr = read(ifd, buf, sizeof(buf))) > 0) {
		nw = fwrite(buf, 1, nr, fp);
		if (nw != nr) {
			syslog(LOG_ERR, "%s: %m", path);
			syslog(LOG_WARNING,
			    "WARNING: kernel may be incomplete");
			exit(1);
		}
	}
	if (nr < 0) {
		syslog(LOG_ERR, "%s: %m", kernel ? kernel : getbootfile());
		syslog(LOG_WARNING,
		    "WARNING: kernel may be incomplete");
		exit(1);
	}
	(void)fclose(fp);
	close(ifd);
}

/*
 * Verify that the specified device node exists and matches the
 * specified device.
 */
int
verify_dev(name, dev)
	char *name;
	register dev_t dev;
{
	struct stat sb;

	if (lstat(name, &sb) == -1)
		return (-1);
	if (!S_ISCHR(sb.st_mode) || sb.st_rdev != dev)
		return (-1);
	return (0);
}

/*
 * Find the dump device.
 *
 *  1) try devname(3); see if it returns something sensible
 *  2) scan /dev for the desired node
 *  3) as a last resort, try to create the node we need
 */
void
find_dev(dev)
	register dev_t dev;
{
	struct dirent *ent;
	char *dn, *dnp;
	DIR *d;

	strcpy(ddname, _PATH_DEV);
	dnp = ddname + sizeof _PATH_DEV - 1;
	if ((dn = devname(dev, S_IFCHR)) != NULL) {
		strcpy(dnp, dn);
		if (verify_dev(ddname, dev) == 0)
			return;
	}
	if ((d = opendir(_PATH_DEV)) != NULL) {
		while ((ent = readdir(d))) {
			strcpy(dnp, ent->d_name);
			if (verify_dev(ddname, dev) == 0) {
				closedir(d);
				return;
			}
		}
		closedir(d);
	}
	strcpy(dnp, "dump");
	if (mknod(ddname, S_IFCHR|S_IRUSR|S_IWUSR, dev) == 0)
		return;
	syslog(LOG_ERR, "can't find device %d/%#x", major(dev), minor(dev));
	exit(1);
}

/*
 * Extract the date and time of the crash from the dump header, and
 * make sure it looks sane (within one week of current date and time).
 */
int
get_crashtime()
{
	time_t dumptime;			/* Time the dump was taken. */

	DumpRead(dumpfd, &dumptime, sizeof(dumptime),
	    (off_t)(dumplo + ok(dump_nl[X_TIME].n_value)), L_SET);
	if (dumptime == 0) {
		if (verbose)
			syslog(LOG_ERR, "dump time is zero");
		return (0);
	}
	(void)printf("savecore: system went down at %s", ctime(&dumptime));
#define	LEEWAY	(7 * 86400)
	if (dumptime < now - LEEWAY || dumptime > now + LEEWAY) {
		(void)printf("dump time is unreasonable\n");
		return (0);
	}
	return (1);
}

/*
 * Extract the size of the dump from the dump header.
 */
void
get_dumpsize()
{
	int kdumpsize;

	/* Read the dump size. */
	DumpRead(dumpfd, &kdumpsize, sizeof(kdumpsize),
	    (off_t)(dumplo + ok(dump_nl[X_DUMPSIZE].n_value)), L_SET);
	dumpsize = (off_t)kdumpsize * getpagesize();
}

/*
 * Check that sufficient space is available on the disk that holds the
 * save directory.
 */
int
check_space()
{
	register FILE *fp;
	const char *tkernel;
	off_t minfree, spacefree, totfree, kernelsize, needed;
	struct stat st;
	struct statfs fsbuf;
	char buf[100], path[MAXPATHLEN];

	tkernel = kernel ? kernel : getbootfile();
	if (stat(tkernel, &st) < 0) {
		syslog(LOG_ERR, "%s: %m", tkernel);
		exit(1);
	}
	kernelsize = st.st_blocks * S_BLKSIZE;

	if (statfs(savedir, &fsbuf) < 0) {
		syslog(LOG_ERR, "%s: %m", savedir);
		exit(1);
	}
 	spacefree = ((off_t) fsbuf.f_bavail * fsbuf.f_bsize) / 1024;
	totfree = ((off_t) fsbuf.f_bfree * fsbuf.f_bsize) / 1024;

	(void)snprintf(path, sizeof(path), "%s/minfree", savedir);
	if ((fp = fopen(path, "r")) == NULL)
		minfree = 0;
	else {
		if (fgets(buf, sizeof(buf), fp) == NULL)
			minfree = 0;
		else
			minfree = atoi(buf);
		(void)fclose(fp);
	}

	needed = (dumpsize + kernelsize) / 1024;
 	if (((minfree > 0) ? spacefree : totfree) - needed < minfree) {
		syslog(LOG_WARNING,
	"no dump, not enough free space on device (%lld available, need %lld)",
		    (long long)(minfree > 0 ? spacefree : totfree),
		    (long long)needed);
		return (0);
	}
	if (spacefree - needed < 0)
		syslog(LOG_WARNING,
		    "dump performed, but free space threshold crossed");
	return (1);
}

int
Open(name, rw)
	const char *name;
	int rw;
{
	int fd;

	if ((fd = open(name, rw, 0)) < 0) {
		syslog(LOG_ERR, "%s: %m", name);
		exit(1);
	}
	return (fd);
}

int
Read(fd, bp, size)
	int fd, size;
	void *bp;
{
	int nr;

	nr = read(fd, bp, size);
	if (nr != size) {
		syslog(LOG_ERR, "read: %m");
		exit(1);
	}
	return (nr);
}

void
Lseek(fd, off, flag)
	int fd, flag;
	off_t off;
{
	off_t ret;

	ret = lseek(fd, off, flag);
	if (ret == -1) {
		syslog(LOG_ERR, "lseek: %m");
		exit(1);
	}
}

/*
 * DumpWrite and DumpRead block io requests to the * dump device.
 */
#define DUMPBUFSIZE	8192
void
DumpWrite(fd, bp, size, off, flag)
	int fd, size, flag;
	void *bp;
	off_t off;
{
	unsigned char buf[DUMPBUFSIZE], *p, *q;
	off_t pos;
	int i, j;
	
	if (flag != L_SET) {
		syslog(LOG_ERR, "lseek: not LSET");
		exit(2);
	}
	q = bp;
	while (size) {
		pos = off & ~(DUMPBUFSIZE - 1);
		Lseek(fd, pos, flag);
		(void)Read(fd, buf, sizeof(buf));
		j = off & (DUMPBUFSIZE - 1);
		p = buf + j;
		i = size;
		if (i > DUMPBUFSIZE - j)
			i = DUMPBUFSIZE - j;
		memcpy(p, q, i);
		Lseek(fd, pos, flag);
		(void)Write(fd, buf, sizeof(buf));
		size -= i;
		q += i;
		off += i;
	}
}

void
DumpRead(fd, bp, size, off, flag)
	int fd, size, flag;
	void *bp;
	off_t off;
{
	unsigned char buf[DUMPBUFSIZE], *p, *q;
	off_t pos;
	int i, j;
	
	if (flag != L_SET) {
		syslog(LOG_ERR, "lseek: not LSET");
		exit(2);
	}
	q = bp;
	while (size) {
		pos = off & ~(DUMPBUFSIZE - 1);
		Lseek(fd, pos, flag);
		(void)Read(fd, buf, sizeof(buf));
		j = off & (DUMPBUFSIZE - 1);
		p = buf + j;
		i = size;
		if (i > DUMPBUFSIZE - j)
			i = DUMPBUFSIZE - j;
		memcpy(q, p, i);
		size -= i;
		q += i;
		off += i;
	}
}

void
Write(fd, bp, size)
	int fd, size;
	void *bp;
{
	int n;

	if ((n = write(fd, bp, size)) < size) {
		syslog(LOG_ERR, "write: %m");
		exit(1);
	}
}

void
usage()
{
	(void)syslog(LOG_ERR, "usage: savecore [-cfkvz] [-N system] directory");
	exit(1);
}
