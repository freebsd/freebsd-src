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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/disk.h>
#include <sys/kerneldump.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <fstab.h>
#include <md5.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

int clear, force, keep, verbose;	/* flags */
int nfound, nsaved, nerr;		/* statistics */

static void
printheader(FILE *f, const struct kerneldumpheader *h, const char *device,
    const char *md5)
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
	fprintf(f, "  MD5: %s\n", md5);
	fflush(f);
}

/*
 * Check that sufficient space is available on the disk that holds the
 * save directory.
 */
static int
check_space(char *savedir, off_t dumpsize)
{
	FILE *fp;
	const char *tkernel;
	off_t minfree, spacefree, totfree, kernelsize, needed;
	struct stat st;
	struct statfs fsbuf;
	char buf[100], path[MAXPATHLEN];

	tkernel = getbootfile();
	if (stat(tkernel, &st) < 0)
		err(1, "%s", tkernel);
	kernelsize = st.st_blocks * S_BLKSIZE;

	if (statfs(savedir, &fsbuf) < 0)
		err(1, "%s", savedir);
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
		warnx("no dump, not enough free space on device"
		    " (%lld available, need %lld)",
		    (long long)(minfree > 0 ? spacefree : totfree),
		    (long long)needed);
		return (0);
	}
	if (spacefree - needed < 0)
		warnx("dump performed, but free space threshold crossed");
	return (1);
}



static void
DoFile(char *savedir, const char *device)
{
	struct kerneldumpheader kdhf, kdhl;
	char buf[BUFSIZ];
	struct stat sb;
	off_t mediasize, dumpsize, firsthd, lasthd;
	char *md5;
	FILE *info;
	int fd, fdcore, fdinfo, error, wl;
	u_int sectorsize;

	if (verbose)
		printf("Checking for kernel dump on device %s\n", device);

	mediasize = 0;
	fd = open(device, O_RDWR);
	if (fd < 0) {
		warn("%s", device);
		return;
	}
	error = ioctl(fd, DIOCGMEDIASIZE, &mediasize);
	if (!error)
		error = ioctl(fd, DIOCGSECTORSIZE, &sectorsize);
	if (error) {
		warn("couldn't find media and/or sector size of %s", device);
		goto closefd;
	}

	if (verbose) {
		printf("Mediasize = %lld\n", (long long)mediasize);
		printf("Sectorsize = %u\n", sectorsize);
	}

	lasthd = mediasize - sectorsize;
	lseek(fd, lasthd, SEEK_SET);
	error = read(fd, &kdhl, sizeof kdhl);
	if (error != sizeof kdhl) {
		warn("error reading last dump header at offset %lld in %s",
		    (long long)lasthd, device);
		goto closefd;
	}
	if (memcmp(kdhl.magic, KERNELDUMPMAGIC, sizeof kdhl.magic)) {
		if (verbose)
			warnx("magic mismatch on last dump header on %s",
			    device);
		goto closefd;
	}
	if (dtoh32(kdhl.version) != KERNELDUMPVERSION) {
		warnx("unknown version (%d) in last dump header on %s",
		    dtoh32(kdhl.version), device);
		goto closefd;
	}

	nfound++;
	if (clear)
		goto nuke;

	if (kerneldump_parity(&kdhl)) {
		warnx("parity error on last dump header on %s", device);
		nerr++;
		goto closefd;
	}
	dumpsize = dtoh64(kdhl.dumplength);
	firsthd = lasthd - dumpsize - sizeof kdhf;
	lseek(fd, firsthd, SEEK_SET);
	error = read(fd, &kdhf, sizeof kdhf);
	if (error != sizeof kdhf) {
		warn("error reading first dump header at offset %lld in %s",
		    (long long)firsthd, device);
		nerr++;
		goto closefd;
	}
	if (memcmp(&kdhl, &kdhf, sizeof kdhl)) {
		warn("first and last dump headers disagree on %s", device);
		nerr++;
		goto closefd;
	}
	md5 = MD5Data((unsigned char *)&kdhl, sizeof kdhl, NULL);
	sprintf(buf, "%s.info", md5);

	/*
	 * See if the dump has been saved already. Don't save the dump
	 * again, unless 'force' is in effect.
	 */
	if (stat(buf, &sb) == 0) {
		if (!force) {
			if (verbose)
				printf("Dump on device %s already saved\n",
				    device);
			goto closefd;
		}
	} else if (errno != ENOENT) {
		warn("error while checking for pre-saved core file");
		nerr++;
		goto closefd;
	}

	if (!check_space(savedir, dumpsize)) {
		nerr++;
		goto closefd;
	}
	/*
	 * Create or overwrite any existing files.
	 */
	fdinfo = open(buf, O_WRONLY | O_CREAT | O_TRUNC, 0600);
	if (fdinfo < 0) {
		warn("%s", buf);
		nerr++;
		goto closefd;
	}
	sprintf(buf, "%s.core", md5);
	fdcore = open(buf, O_WRONLY | O_CREAT | O_TRUNC, 0600);
	if (fdcore < 0) {
		warn("%s", buf);
		close(fdinfo);
		nerr++;
		goto closefd;
	}
	info = fdopen(fdinfo, "w");

	if (verbose)
		printheader(stdout, &kdhl, device, md5);

	printf("Saving dump to file %s\n", buf);

	printheader(info, &kdhl, device, md5);

	while (dumpsize > 0) {
		wl = sizeof(buf);
		if (wl > dumpsize)
			wl = dumpsize;
		error = read(fd, buf, wl);
		if (error != wl) {
			warn("read error on %s", device);
			nerr++;
			goto closeall;
		}
		error = write(fdcore, buf, wl);
		if (error != wl) {
			warn("write error on %s.core file", md5);
			nerr++;
			goto closeall;
		}
		dumpsize -= wl;
	}
	nsaved++;
	close(fdinfo);
	close(fdcore);

	if (verbose)
		printf("Dump saved\n");

 nuke:
	if (clear || !keep) {
		if (verbose)
			printf("Clearing dump header\n");
		memset(&kdhl, 0, sizeof kdhl);
		lseek(fd, lasthd, SEEK_SET);
		error = write(fd, &kdhl, sizeof kdhl);
		if (error != sizeof kdhl)
			warn("error while clearing the dump header");
	}
	close(fd);
	return;

 closeall:
	close(fdinfo);
	close(fdcore);

 closefd:
	close(fd);
}

static void
usage(void)
{
	fprintf(stderr, "usage: savecore [-cfkv] [directory [device...]]\n");
	exit (1);
}

int
main(int argc, char **argv)
{
	int i, ch, error;
	struct fstab *fsp;
	char *savedir;

	savedir = strdup(".");
	if (savedir == NULL)
		errx(1, "Cannot allocate memory");
	while ((ch = getopt(argc, argv, "cdfkN:vz")) != -1)
		switch(ch) {
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
		case 'd':	/* Obsolete */
		case 'N':
		case 'z':
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;
	if (argc >= 1) {
		error = chdir(argv[0]);
		if (error)
			err(1, "chdir(%s)", argv[0]);
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
	if (nfound == 0)
		printf("No dumps found\n");
	else if (nsaved == 0) {
		if (nerr != 0)
			printf("Unsaved dumps found but not saved\n");
		else
			printf("No unsaved dumps found\n");
	}

	return (0);
}
