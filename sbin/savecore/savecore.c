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

int checkfor, compress, clear, force, keep, verbose;	/* flags */
int nfound, nsaved, nerr;			/* statistics */

extern FILE *zopen(const char *, const char *);

static void
printheader(FILE *f, const struct kerneldumpheader *h, const char *device,
    int bounds)
{
	uint64_t dumplen;
	time_t t;

	fprintf(f, "Good dump found on device %s\n", device);
	fprintf(f, "  Architecture: %s\n", h->architecture);
	fprintf(f, "  Architecture version: %d\n",
	    dtoh32(h->architectureversion));
	dumplen = dtoh64(h->dumplength);
	fprintf(f, "  Dump length: %lldB (%lld MB)\n", (long long)dumplen,
	    (long long)(dumplen >> 20));
	fprintf(f, "  Blocksize: %d\n", dtoh32(h->blocksize));
	t = dtoh64(h->dumptime);
	fprintf(f, "  Dumptime: %s", ctime(&t));
	fprintf(f, "  Hostname: %s\n", h->hostname);
	fprintf(f, "  Versionstring: %s", h->versionstring);
	fprintf(f, "  Panicstring: %s\n", h->panicstring);
	fprintf(f, "  Bounds: %d\n", bounds);
	fflush(f);
}

static int
getbounds(void) {
	FILE *fp;
	char buf[6];
	int ret;

	ret = 0;

	if ((fp = fopen("bounds", "r")) == NULL) {
		syslog(LOG_WARNING, "unable to open bounds file, using 0");
		goto newfile;
	}

	if (fgets(buf, sizeof buf, fp) == NULL) {
		syslog(LOG_WARNING, "unable to read from bounds, using 0");
		fclose(fp);
		goto newfile;
	}

	errno = 0;
	ret = (int)strtol(buf, NULL, 10);
	if (ret == 0 && (errno == EINVAL || errno == ERANGE))
		syslog(LOG_WARNING, "invalid value found in bounds, using 0");

newfile:

	if ((fp = fopen("bounds", "w")) == NULL) {
		syslog(LOG_WARNING, "unable to write to bounds file: %m");
		goto done;
	}

	if (verbose)
		printf("bounds number: %d\n", ret);

	fprintf(fp, "%d\n", (ret + 1));
	fclose(fp);

done:
	return (ret);
}

/*
 * Check that sufficient space is available on the disk that holds the
 * save directory.
 */
static int
check_space(char *savedir, off_t dumpsize)
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

static void
DoFile(char *savedir, const char *device)
{
	static char *buf = NULL;
	struct kerneldumpheader kdhf, kdhl;
	off_t mediasize, dumpsize, firsthd, lasthd, dmpcnt;
	FILE *info, *fp;
	int fd, fdinfo, error, wl;
	int nr, nw, hs, he;
	int bounds;
	u_int sectorsize;
	mode_t oumask;

	dmpcnt = 0;
	mediasize = 0;

	/*
	 * XXX On ia64 something breaks when the buffer is put on the
	 * stack. When the buffer is roughly larger than 128K the read()
	 * below simply fails with errno=14 (EFAULT). We work around
	 * this by doing a one-time allocation...
	 */
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
	if (memcmp(kdhl.magic, KERNELDUMPMAGIC, sizeof kdhl.magic)) {
		if (verbose)
			printf("magic mismatch on last dump header on %s\n",
			    device);

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
	}
	if (dtoh32(kdhl.version) != KERNELDUMPVERSION) {
		syslog(LOG_ERR,
		    "unknown version (%d) in last dump header on %s",
		    dtoh32(kdhl.version), device);
		goto closefd;
	}

	nfound++;
	if (clear)
		goto nuke;

	if (kerneldump_parity(&kdhl)) {
		syslog(LOG_ERR,
		    "parity error on last dump header on %s", device);
		nerr++;
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
	if (memcmp(&kdhl, &kdhf, sizeof kdhl)) {
		syslog(LOG_ERR,
		    "first and last dump headers disagree on %s", device);
		nerr++;
		goto closefd;
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

	bounds = getbounds();

	sprintf(buf, "info.%d", bounds);

	/*
	 * Create or overwrite any existing files.
	 */
	fdinfo = open(buf, O_WRONLY | O_CREAT | O_TRUNC, 0600);
	if (fdinfo < 0) {
		syslog(LOG_ERR, "%s: %m", buf);
		nerr++;
		goto closefd;
	}
	oumask = umask(S_IRWXG|S_IRWXO); /* Restrict access to the core file.*/
	if (compress) {
		sprintf(buf, "vmcore.%d.gz", bounds);
		fp = zopen(buf, "w");
	} else {
		sprintf(buf, "vmcore.%d", bounds);
		fp = fopen(buf, "w");
	}
	if (fp == NULL) {
		syslog(LOG_ERR, "%s: %m", buf);
		close(fdinfo);
		nerr++;
		goto closefd;
	}
	(void)umask(oumask);

	info = fdopen(fdinfo, "w");

	if (verbose)
		printheader(stdout, &kdhl, device, bounds);

	printheader(info, &kdhl, device, bounds);
	fclose(info);

	syslog(LOG_NOTICE, "writing %score to %s",
	    compress ? "compressed " : "", buf);

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
			goto closeall;
		}
		if (compress) {
			nw = fwrite(buf, 1, wl, fp);
		} else {
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
		}
		if (nw != wl) {
			syslog(LOG_ERR,
			    "write error on vmcore.%d file: %m", bounds);
			syslog(LOG_WARNING,
			    "WARNING: vmcore may be incomplete");
			nerr++;
			goto closeall;
		}
		if (verbose) {
			dmpcnt += wl;
			printf("%llu\r", (unsigned long long)dmpcnt);
			fflush(stdout);
		}
		dumpsize -= wl;
	}
	if (verbose)
		printf("\n");

	if (fclose(fp) < 0) {
		syslog(LOG_ERR, "error on vmcore.%d: %m", bounds);
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
	fprintf(stderr, "usage: savecore [-Cv|-cfkv] [directory [device...]]\n");
	exit (1);
}

int
main(int argc, char **argv)
{
	int i, ch, error;
	struct fstab *fsp;
	char *savedir;

	openlog("savecore", LOG_PERROR, LOG_DAEMON);

	savedir = strdup(".");
	if (savedir == NULL) {
		syslog(LOG_ERR, "Cannot allocate memory");
		exit(1);
	}
	while ((ch = getopt(argc, argv, "CcdfkN:vz")) != -1)
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
			verbose = 1;
			break;
		case 'f':
			force = 1;
			break;
		case 'z':
			compress = 1;
			break;
		case 'd':	/* Obsolete */
		case 'N':
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
