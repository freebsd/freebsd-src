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
 * $FreeBSD$
 */

#include <sys/types.h>
#include <sys/disk.h>
#include <sys/kerneldump.h>
#include <sys/stat.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <fstab.h>
#include <md5.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

int clear, force, keep, verbose;	/* flags */
int nfound, nsaved;			/* statistics */

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
}


static void
DoFile(const char *device)
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
		warn("Couldn't find media and/or sector size of %s)", device);
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
		warn("Error reading last dump header at offset %lld in %s",
		    (long long)lasthd, device);
		goto closefd;
	}
	if (memcmp(kdhl.magic, KERNELDUMPMAGIC, sizeof kdhl.magic)) {
		if (verbose)
			warnx("Magic mismatch on last dump header on %s",
			    device);
		goto closefd;
	}
	if (dtoh32(kdhl.version) != KERNELDUMPVERSION) {
		warnx("Unknown version (%d) in last dump header on %s",
		    dtoh32(kdhl.version), device);
		goto closefd;
	}

	nfound++;
	if (clear)
		goto nuke;

	if (kerneldump_parity(&kdhl)) {
		warnx("Parity error on last dump header on %s", device);
		goto closefd;
	}
	dumpsize = dtoh64(kdhl.dumplength);
	firsthd = lasthd - dumpsize - sizeof kdhf;
	lseek(fd, firsthd, SEEK_SET);
	error = read(fd, &kdhf, sizeof kdhf);
	if (error != sizeof kdhf) {
		warn("Error reading first dump header at offset %lld in %s",
		    (long long)firsthd, device);
		goto closefd;
	}
	if (memcmp(&kdhl, &kdhf, sizeof kdhl)) {
		warn("First and last dump headers disagree on %s", device);
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
		warn("Error while checking for pre-saved core file");
		goto closefd;
	}

	/*
	 * Create or overwrite any existing files.
	 */
	fdinfo = open(buf, O_WRONLY | O_CREAT | O_TRUNC, 0600);
	if (fdinfo < 0) {
		warn("%s", buf);
		goto closefd;
	}
	sprintf(buf, "%s.core", md5);
	fdcore = open(buf, O_WRONLY | O_CREAT | O_TRUNC, 0600);
	if (fdcore < 0) {
		warn("%s", buf);
		close(fdinfo);
		goto closefd;
	}
	info = fdopen(fdinfo, "w");

	if (verbose)
		printheader(stdout, &kdhl, device, md5);

	printf("Saving dump to file %s\n", buf);
	nsaved++;

	printheader(info, &kdhl, device, md5);

	while (dumpsize > 0) {
		wl = sizeof(buf);
		if (wl > dumpsize)
			wl = dumpsize;
		error = read(fd, buf, wl);
		if (error != wl) {
			warn("Read error on %s", device);
			goto closeall;
		}
		error = write(fdcore, buf, wl);
		if (error != wl) {
			warn("Write error on %s.core file", md5);
			goto closeall;
		}
		dumpsize -= wl;
	}
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
			warn("Error while clearing the dump header");
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
	errx(1, "usage: savecore [-cfkv] [directory [device...]]");
	exit (1);
}

int
main(int argc, char **argv)
{
	int i, ch, error;
	struct fstab *fsp;

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
			DoFile(fsp->fs_spec);
		}
	} else {
		for (i = 0; i < argc; i++)
			DoFile(argv[i]);
	}

	/* Emit minimal output. */
	if (nfound == 0)
		printf("No dumps found\n");
	else if (nsaved == 0)
		printf("No unsaved dumps found\n");

	return (0);
}
