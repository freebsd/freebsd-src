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

#include <stdio.h>
#include <unistd.h>
#include <err.h>
#include <fcntl.h>
#include <fstab.h>
#include <errno.h>
#include <time.h>
#include <md5.h>
#include <unistd.h>
#include <sys/disklabel.h>
#include <sys/kerneldump.h>

static void
printheader(FILE *f, const struct kerneldumpheader *h, const char *devname,
    const char *md5)
{
	uint64_t dumplen;
	time_t t;

	fprintf(f, "Good dump found on device %s\n", devname);
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
DoFile(const char *devname)
{
	int fd, fdcore, fdinfo, error, wl;
	off_t mediasize, dumpsize, firsthd, lasthd;
	u_int sectorsize;
	struct kerneldumpheader kdhf, kdhl;
	char *md5;
	FILE *info;
	char buf[BUFSIZ];

	mediasize = 0;
	fd = open(devname, O_RDONLY);
	if (fd < 0) {
		warn("%s", devname);
		return;
	}
	error = ioctl(fd, DIOCGMEDIASIZE, &mediasize);
	if (!error)
		error = ioctl(fd, DIOCGSECTORSIZE, &sectorsize);
	if (error) {
		warn("Couldn't find media and/or sector size of %s)", devname);
		return;
	}
	printf("Mediasize = %lld\n", (long long)mediasize);
	printf("Sectorsize = %u\n", sectorsize);
	lasthd = mediasize - sectorsize;
	lseek(fd, lasthd, SEEK_SET);
	error = read(fd, &kdhl, sizeof kdhl);
	if (error != sizeof kdhl) {
		warn("Error Reading last dump header at offset %lld in %s",
		    (long long)lasthd, devname);
		return;
	}
	if (kerneldump_parity(&kdhl)) {
		warnx("Parity error on last dump header on %s\n", devname);
		return;
	}
	if (memcmp(kdhl.magic, KERNELDUMPMAGIC, sizeof kdhl.magic)) {
		warnx("Magic mismatch on last dump header on %s\n", devname);
		return;
	}
	if (dtoh32(kdhl.version) != KERNELDUMPVERSION) {
		warnx("Unknown version (%d) in last dump header on %s\n",
		    dtoh32(kdhl.version), devname);
		return;
	}
	dumpsize = dtoh64(kdhl.dumplength);
	firsthd = lasthd - dumpsize - sizeof kdhf;
	lseek(fd, firsthd, SEEK_SET);
	error = read(fd, &kdhf, sizeof kdhf);
	if (error != sizeof kdhf) {
		warn("Error Reading first dump header at offset %lld in %s",
		    (long long)firsthd, devname);
		return;
	}
	if (memcmp(&kdhl, &kdhf, sizeof kdhl)) {
		warn("First and last dump headers disagree on %s\n", devname);
		return;
	}
	md5 = MD5Data((unsigned char *)&kdhl, sizeof kdhl, NULL);
	sprintf(buf, "%s.info", md5);
	fdinfo = open(buf, O_WRONLY | O_CREAT | O_EXCL, 0600);
	if (fdinfo < 0 && errno == EEXIST) {
		printf("Dump on device %s already saved\n", devname);
		return;
	}
	if (fdinfo < 0) {
		warn("%s", buf);
		return;
	}
	sprintf(buf, "%s.core", md5);
	fdcore = open(buf, O_WRONLY | O_CREAT | O_EXCL, 0600);
	if (fdcore < 0) {
		warn("%s", buf);
		return;
	}
	info = fdopen(fdinfo, "w");
	printheader(stdout, &kdhl, devname, md5);
	printheader(info, &kdhl, devname, md5);
	printf("Saving dump to file...\n");
	while (dumpsize > 0) {
		wl = sizeof(buf);
		if (wl > dumpsize)
			wl = dumpsize;
		error = read(fd, buf, wl);
		if (error != wl) {
			warn("read error on %s\n", devname);	
			return;
		}
		error = write(fdcore, buf, wl);
		if (error != wl) {
			warn("write error on %s.core file\n", md5);	
			return;
		}
		dumpsize -= wl;
	}
	close (fdinfo);
	close (fdcore);
	printf("Dump saved\n");
}

static void
usage(void)
{
	errx(1, "usage: ...");
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
		case 'd':
		case 'v':
		case 'f':
		case 'k':
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
	return (0);
}
