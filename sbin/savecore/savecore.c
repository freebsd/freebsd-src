/*-
 * Copyright (c) 2002 Poul-Henning Kamp
 * Copyright (c) 2002 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project by Poul-Henning Kamp
 * and NAI Labs, the Security Research Division of Network Associates, Inc.
 * under DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"), as part of the
 * DARPA CHATS research program.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The names of the authors may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/disk.h>
#include <sys/kerneldump.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <fstab.h>
#include <paths.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

/* The size of the buffer used for I/O. */
#define	BUFFERSIZE	(1024*1024)

#define	STATUS_BAD	0
#define	STATUS_GOOD	1
#define	STATUS_UNKNOWN	2

static int checkfor, compress, clear, force, keep, verbose;	/* flags */
static int nfound, nsaved, nerr;			/* statistics */

extern FILE *zopen(const char *, const char *);

static sig_atomic_t got_siginfo;
static void infohandler(int);

static void
printheader(FILE *f, const struct kerneldumpheader *h, const char *device,
    int bounds, const int status)
{
	uint64_t dumplen;
	time_t t;
	const char *stat_str;

	fprintf(f, "Dump header from device %s\n", device);
	fprintf(f, "  Architecture: %s\n", h->architecture);
	fprintf(f, "  Architecture Version: %u\n",
	    dtoh32(h->architectureversion));
	dumplen = dtoh64(h->dumplength);
	fprintf(f, "  Dump Length: %lldB (%lld MB)\n", (long long)dumplen,
	    (long long)(dumplen >> 20));
	fprintf(f, "  Blocksize: %d\n", dtoh32(h->blocksize));
	t = dtoh64(h->dumptime);
	fprintf(f, "  Dumptime: %s", ctime(&t));
	fprintf(f, "  Hostname: %s\n", h->hostname);
	fprintf(f, "  Magic: %s\n", h->magic);
	fprintf(f, "  Version String: %s", h->versionstring);
	fprintf(f, "  Panic String: %s\n", h->panicstring);
	fprintf(f, "  Dump Parity: %u\n", h->parity);
	fprintf(f, "  Bounds: %d\n", bounds);

	switch(status) {
	case STATUS_BAD:
		stat_str = "bad";
		break;
	case STATUS_GOOD:
		stat_str = "good";
		break;
	default:
		stat_str = "unknown";
	}
	fprintf(f, "  Dump Status: %s\n", stat_str);
	fflush(f);
}

static int
getbounds(void) {
	FILE *fp;
	char buf[6];
	int ret;

	ret = 0;

	if ((fp = fopen("bounds", "r")) == NULL) {
		if (verbose)
			printf("unable to open bounds file, using 0\n");
		return (ret);
	}

	if (fgets(buf, sizeof buf, fp) == NULL) {
		syslog(LOG_WARNING, "unable to read from bounds, using 0");
		fclose(fp);
		return (ret);
	}

	errno = 0;
	ret = (int)strtol(buf, NULL, 10);
	if (ret == 0 && (errno == EINVAL || errno == ERANGE))
		syslog(LOG_WARNING, "invalid value found in bounds, using 0");
	return (ret);
}

static void
writebounds(int bounds) {
	FILE *fp;

	if ((fp = fopen("bounds", "w")) == NULL) {
		syslog(LOG_WARNING, "unable to write to bounds file: %m");
		return;
	}

	if (verbose)
		printf("bounds number: %d\n", bounds);

	fprintf(fp, "%d\n", bounds);
	fclose(fp);
}

/*
 * Check that sufficient space is available on the disk that holds the
 * save directory.
 */
static int
check_space(const char *savedir, off_t dumpsize)
{
	FILE *fp;
	off_t minfree, spacefree, totfree, needed;
	struct statfs fsbuf;
	char buf[100], path[MAXPATHLEN];

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

	needed = dumpsize / 1024 + 2;	/* 2 for info file */
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

#define BLOCKSIZE (1<<12)
#define BLOCKMASK (~(BLOCKSIZE-1))

static int
DoRegularFile(int fd, off_t dumpsize, char *buf, const char *device,
    const char *filename, FILE *fp)
{
	int he, hs, nr, nw, wl;
	off_t dmpcnt, origsize;

	dmpcnt = 0;
	origsize = dumpsize;
	he = 0;
	while (dumpsize > 0) {
		wl = BUFFERSIZE;
		if (wl > dumpsize)
			wl = dumpsize;
		nr = read(fd, buf, wl);
		if (nr != wl) {
			if (nr == 0)
				syslog(LOG_WARNING,
				    "WARNING: EOF on dump device");
			else
				syslog(LOG_ERR, "read error on %s: %m", device);
			nerr++;
			return (-1);
		}
		if (compress) {
			nw = fwrite(buf, 1, wl, fp);
		} else {
			for (nw = 0; nw < nr; nw = he) {
				/* find a contiguous block of zeroes */
				for (hs = nw; hs < nr; hs += BLOCKSIZE) {
					for (he = hs; he < nr && buf[he] == 0;
					    ++he)
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
					if (fwrite(buf + nw, hs - nw, 1, fp)
					    != 1)
					break;
				if (he > hs)
					if (fseeko(fp, he - hs, SEEK_CUR) == -1)
						break;
			}
		}
		if (nw != wl) {
			syslog(LOG_ERR,
			    "write error on %s file: %m", filename);
			syslog(LOG_WARNING,
			    "WARNING: vmcore may be incomplete");
			nerr++;
			return (-1);
		}
		if (verbose) {
			dmpcnt += wl;
			printf("%llu\r", (unsigned long long)dmpcnt);
			fflush(stdout);
		}
		dumpsize -= wl;
		if (got_siginfo) {
			printf("%s %.1lf%%\n", filename, (100.0 - (100.0 *
			    (double)dumpsize / (double)origsize)));
			got_siginfo = 0;
		}
	}
	return (0);
}

/*
 * Specialized version of dump-reading logic for use with textdumps, which
 * are written backwards from the end of the partition, and must be reversed
 * before being written to the file.  Textdumps are small, so do a bit less
 * work to optimize/sparsify.
 */
static int
DoTextdumpFile(int fd, off_t dumpsize, off_t lasthd, char *buf,
    const char *device, const char *filename, FILE *fp)
{
	int nr, nw, wl;
	off_t dmpcnt, totsize;

	totsize = dumpsize;
	dmpcnt = 0;
	wl = 512;
	if ((dumpsize % wl) != 0) {
		syslog(LOG_ERR, "textdump uneven multiple of 512 on %s",
		    device);
		nerr++;
		return (-1);
	}
	while (dumpsize > 0) {
		nr = pread(fd, buf, wl, lasthd - (totsize - dumpsize) - wl);
		if (nr != wl) {
			if (nr == 0)
				syslog(LOG_WARNING,
				    "WARNING: EOF on dump device");
			else
				syslog(LOG_ERR, "read error on %s: %m", device);
			nerr++;
			return (-1);
		}
		nw = fwrite(buf, 1, wl, fp);
		if (nw != wl) {
			syslog(LOG_ERR,
			    "write error on %s file: %m", filename);
			syslog(LOG_WARNING,
			    "WARNING: textdump may be incomplete");
			nerr++;
			return (-1);
		}
		if (verbose) {
			dmpcnt += wl;
			printf("%llu\r", (unsigned long long)dmpcnt);
			fflush(stdout);
		}
		dumpsize -= wl;
	}
	return (0);
}

static void
DoFile(const char *savedir, const char *device)
{
	static char filename[PATH_MAX];
	static char *buf = NULL;
	struct kerneldumpheader kdhf, kdhl;
	off_t mediasize, dumpsize, firsthd, lasthd;
	FILE *info, *fp;
	mode_t oumask;
	int fd, fdinfo, error;
	int bounds, status;
	u_int sectorsize;
	int istextdump;

	bounds = getbounds();
	mediasize = 0;
	status = STATUS_UNKNOWN;

	if (buf == NULL) {
		buf = malloc(BUFFERSIZE);
		if (buf == NULL) {
			syslog(LOG_ERR, "%m");
			return;
		}
	}

	if (verbose)
		printf("checking for kernel dump on device %s\n", device);

	fd = open(device, O_RDWR);
	if (fd < 0) {
		syslog(LOG_ERR, "%s: %m", device);
		return;
	}

	error = ioctl(fd, DIOCGMEDIASIZE, &mediasize);
	if (!error)
		error = ioctl(fd, DIOCGSECTORSIZE, &sectorsize);
	if (error) {
		syslog(LOG_ERR,
		    "couldn't find media and/or sector size of %s: %m", device);
		goto closefd;
	}

	if (verbose) {
		printf("mediasize = %lld\n", (long long)mediasize);
		printf("sectorsize = %u\n", sectorsize);
	}

	lasthd = mediasize - sectorsize;
	lseek(fd, lasthd, SEEK_SET);
	error = read(fd, &kdhl, sizeof kdhl);
	if (error != sizeof kdhl) {
		syslog(LOG_ERR,
		    "error reading last dump header at offset %lld in %s: %m",
		    (long long)lasthd, device);
		goto closefd;
	}
	istextdump = 0;
	if (strncmp(kdhl.magic, TEXTDUMPMAGIC, sizeof kdhl) == 0) {
		if (verbose)
			printf("textdump magic on last dump header on %s\n",
			    device);
		istextdump = 1;
		if (dtoh32(kdhl.version) != KERNELDUMP_TEXT_VERSION) {
			syslog(LOG_ERR,
			    "unknown version (%d) in last dump header on %s",
			    dtoh32(kdhl.version), device);
	
			status = STATUS_BAD;
			if (force == 0)
				goto closefd;
		}
	} else if (memcmp(kdhl.magic, KERNELDUMPMAGIC, sizeof kdhl.magic) ==
	    0) {
		if (dtoh32(kdhl.version) != KERNELDUMPVERSION) {
			syslog(LOG_ERR,
			    "unknown version (%d) in last dump header on %s",
			    dtoh32(kdhl.version), device);
	
			status = STATUS_BAD;
			if (force == 0)
				goto closefd;
		}
	} else {
		if (verbose)
			printf("magic mismatch on last dump header on %s\n",
			    device);

		status = STATUS_BAD;
		if (force == 0)
			goto closefd;

		if (memcmp(kdhl.magic, KERNELDUMPMAGIC_CLEARED,
			    sizeof kdhl.magic) == 0) {
			if (verbose)
				printf("forcing magic on %s\n", device);
			memcpy(kdhl.magic, KERNELDUMPMAGIC,
			    sizeof kdhl.magic);
		} else {
			syslog(LOG_ERR, "unable to force dump - bad magic");
			goto closefd;
		}
		if (dtoh32(kdhl.version) != KERNELDUMPVERSION) {
			syslog(LOG_ERR,
			    "unknown version (%d) in last dump header on %s",
			    dtoh32(kdhl.version), device);
	
			status = STATUS_BAD;
			if (force == 0)
				goto closefd;
		}
	}

	nfound++;
	if (clear)
		goto nuke;

	if (kerneldump_parity(&kdhl)) {
		syslog(LOG_ERR,
		    "parity error on last dump header on %s", device);
		nerr++;
		status = STATUS_BAD;
		if (force == 0)
			goto closefd;
	}
	dumpsize = dtoh64(kdhl.dumplength);
	firsthd = lasthd - dumpsize - sizeof kdhf;
	lseek(fd, firsthd, SEEK_SET);
	error = read(fd, &kdhf, sizeof kdhf);
	if (error != sizeof kdhf) {
		syslog(LOG_ERR,
		    "error reading first dump header at offset %lld in %s: %m",
		    (long long)firsthd, device);
		nerr++;
		goto closefd;
	}

	if (verbose >= 2) {
		printf("First dump headers:\n");
		printheader(stdout, &kdhf, device, bounds, -1);

		printf("\nLast dump headers:\n");
		printheader(stdout, &kdhl, device, bounds, -1);
		printf("\n");
	}

	if (memcmp(&kdhl, &kdhf, sizeof kdhl)) {
		syslog(LOG_ERR,
		    "first and last dump headers disagree on %s", device);
		nerr++;
		status = STATUS_BAD;
		if (force == 0)
			goto closefd;
	} else {
		status = STATUS_GOOD;
	}

	if (checkfor) {
		printf("A dump exists on %s\n", device);
		close(fd);
		exit(0);
	}

	if (kdhl.panicstring[0])
		syslog(LOG_ALERT, "reboot after panic: %s", kdhl.panicstring);
	else
		syslog(LOG_ALERT, "reboot");

	if (verbose)
		printf("Checking for available free space\n");
	if (!check_space(savedir, dumpsize)) {
		nerr++;
		goto closefd;
	}

	writebounds(bounds + 1);

	sprintf(buf, "info.%d", bounds);

	/*
	 * Create or overwrite any existing dump header files.
	 */
	fdinfo = open(buf, O_WRONLY | O_CREAT | O_TRUNC, 0600);
	if (fdinfo < 0) {
		syslog(LOG_ERR, "%s: %m", buf);
		nerr++;
		goto closefd;
	}
	oumask = umask(S_IRWXG|S_IRWXO); /* Restrict access to the core file.*/
	if (compress) {
		sprintf(filename, "%s.%d.gz", istextdump ? "textdump.tar" :
		    "vmcore", bounds);
		fp = zopen(filename, "w");
	} else {
		sprintf(filename, "%s.%d", istextdump ? "textdump.tar" :
		    "vmcore", bounds);
		fp = fopen(filename, "w");
	}
	if (fp == NULL) {
		syslog(LOG_ERR, "%s: %m", filename);
		close(fdinfo);
		nerr++;
		goto closefd;
	}
	(void)umask(oumask);

	info = fdopen(fdinfo, "w");

	if (info == NULL) {
		syslog(LOG_ERR, "fdopen failed: %m");
		nerr++;
		goto closefd;
	}

	if (verbose)
		printheader(stdout, &kdhl, device, bounds, status);

	printheader(info, &kdhl, device, bounds, status);
	fclose(info);

	syslog(LOG_NOTICE, "writing %score to %s",
	    compress ? "compressed " : "", filename);

	if (istextdump) {
		if (DoTextdumpFile(fd, dumpsize, lasthd, buf, device,
		    filename, fp) < 0)
			goto closeall;
	} else {
		if (DoRegularFile(fd, dumpsize, buf, device, filename, fp)
		    < 0)
			goto closeall;
	}
	if (verbose)
		printf("\n");

	if (fclose(fp) < 0) {
		syslog(LOG_ERR, "error on %s: %m", filename);
		nerr++;
		goto closeall;
	}
	nsaved++;

	if (verbose)
		printf("dump saved\n");

nuke:
	if (clear || !keep) {
		if (verbose)
			printf("clearing dump header\n");
		memcpy(kdhl.magic, KERNELDUMPMAGIC_CLEARED, sizeof kdhl.magic);
		lseek(fd, lasthd, SEEK_SET);
		error = write(fd, &kdhl, sizeof kdhl);
		if (error != sizeof kdhl)
			syslog(LOG_ERR,
			    "error while clearing the dump header: %m");
	}
	close(fd);
	return;

closeall:
	fclose(fp);

closefd:
	close(fd);
}

static void
usage(void)
{
	fprintf(stderr, "%s\n%s\n%s\n",
	    "usage: savecore -c",
	    "       savecore -C [-v] [directory device]",
	    "       savecore [-fkvz] [directory [device ...]]");
	exit (1);
}

int
main(int argc, char **argv)
{
	const char *savedir = ".";
	struct fstab *fsp;
	int i, ch, error;

	checkfor = compress = clear = force = keep = verbose = 0;
	nfound = nsaved = nerr = 0;

	openlog("savecore", LOG_PERROR, LOG_DAEMON);
	signal(SIGINFO, infohandler);

	while ((ch = getopt(argc, argv, "Ccfkvz")) != -1)
		switch(ch) {
		case 'C':
			checkfor = 1;
			break;
		case 'c':
			clear = 1;
			break;
		case 'k':
			keep = 1;
			break;
		case 'v':
			verbose++;
			break;
		case 'f':
			force = 1;
			break;
		case 'z':
			compress = 1;
			break;
		case '?':
		default:
			usage();
		}
	if (checkfor && (clear || force || keep))
		usage();
	argc -= optind;
	argv += optind;
	if (argc >= 1) {
		error = chdir(argv[0]);
		if (error) {
			syslog(LOG_ERR, "chdir(%s): %m", argv[0]);
			exit(1);
		}
		savedir = argv[0];
		argc--;
		argv++;
	}
	if (argc == 0) {
		for (;;) {
			fsp = getfsent();
			if (fsp == NULL)
				break;
			if (strcmp(fsp->fs_vfstype, "swap") &&
			    strcmp(fsp->fs_vfstype, "dump"))
				continue;
			DoFile(savedir, fsp->fs_spec);
		}
	} else {
		for (i = 0; i < argc; i++)
			DoFile(savedir, argv[i]);
	}

	/* Emit minimal output. */
	if (nfound == 0) {
		if (checkfor) {
			printf("No dump exists\n");
			exit(1);
		}
		syslog(LOG_WARNING, "no dumps found");
	}
	else if (nsaved == 0) {
		if (nerr != 0)
			syslog(LOG_WARNING, "unsaved dumps found but not saved");
		else
			syslog(LOG_WARNING, "no unsaved dumps found");
	}

	return (0);
}

static void
infohandler(int sig __unused)
{
	got_siginfo = 1;
}
