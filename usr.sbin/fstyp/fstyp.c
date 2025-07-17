/*-
 * Copyright (c) 2014 The FreeBSD Foundation
 *
 * This software was developed by Edward Tomasz Napierala under sponsorship
 * from the FreeBSD Foundation.
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
 */

#include <sys/capsicum.h>
#include <sys/disk.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <capsicum_helpers.h>
#include <err.h>
#include <errno.h>
#ifdef WITH_ICONV
#include <iconv.h>
#endif
#include <locale.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <vis.h>

#include "fstyp.h"

#define	LABEL_LEN	256

bool show_label = false;

typedef int (*fstyp_function)(FILE *, char *, size_t);

/*
 * The ordering of fstypes[] is not arbitrary.
 *
 * fstyp checks the existence of a file system header to determine the
 * type of file system on a given device. For different file systems,
 * these headers reside at different offsets within the device.
 *
 * For example, the header for an MS-DOS file system begins at offset 0,
 * whereas a header for UFS *normally* begins at offset 64k. If a device
 * was constructed as MS-DOS and then repurposed as UFS (via newfs), it
 * is possible the MS-DOS header will still be intact. To prevent
 * misidentifying the file system, it is desirable to check the header
 * with the largest offset first (i.e., UFS before MS-DOS).
 */
static struct {
	const char	*name;
	fstyp_function	function;
	bool		unmountable;
	const char	*precache_encoding;
} fstypes[] = {
	/* last sector of geli device */
	{ "geli", &fstyp_geli, true, NULL },
	/*
	 * ufs headers have four different areas, searched in this order:
	 * offsets: 64k, 8k, 0k, 256k + 8192 bytes
	 */
	{ "ufs", &fstyp_ufs, false, NULL },
	/* offset 32768 + 512 bytes */
	{ "cd9660", &fstyp_cd9660, false, NULL },
	/* offset 1024 + 512 bytes */
	{ "hfs+", &fstyp_hfsp, false, NULL },
	/* offset 1024 + 512 bytes */
	{ "ext2fs", &fstyp_ext2fs, false, NULL },
	/* offset 512 + 36 bytes */
	{ "befs", &fstyp_befs, false, NULL },
	/* offset 0 + 40 bytes */
	{ "apfs", &fstyp_apfs, true, NULL },
	/* offset 0 + 512 bytes (for initial signature check) */
	{ "exfat", &fstyp_exfat, false, EXFAT_ENC },
	/* offset 0 + 1928 bytes */
	{ "hammer", &fstyp_hammer, true, NULL },
	/* offset 0 + 65536 bytes (for initial signature check) */
	{ "hammer2", &fstyp_hammer2, true, NULL },
	/* offset 0 + 512 bytes (for initial signature check) */
	{ "msdosfs", &fstyp_msdosfs, false, NULL },
	/* offset 0 + 512 bytes (for initial signature check) */
	{ "ntfs", &fstyp_ntfs, false, NTFS_ENC },
#ifdef HAVE_ZFS
	/* offset 0 + 256k */
	{ "zfs", &fstyp_zfs, true, NULL },
#endif
	{ NULL, NULL, NULL, NULL }
};

void *
read_buf(FILE *fp, off_t off, size_t len)
{
	int error;
	size_t nread;
	void *buf;

	error = fseek(fp, off, SEEK_SET);
	if (error != 0) {
		warn("cannot seek to %jd", (uintmax_t)off);
		return (NULL);
	}

	buf = malloc(len);
	if (buf == NULL) {
		warn("cannot malloc %zd bytes of memory", len);
		return (NULL);
	}

	nread = fread(buf, len, 1, fp);
	if (nread != 1) {
		free(buf);
		if (feof(fp) == 0)
			warn("fread");
		return (NULL);
	}

	return (buf);
}

char *
checked_strdup(const char *s)
{
	char *c;

	c = strdup(s);
	if (c == NULL)
		err(1, "strdup");
	return (c);
}

void
rtrim(char *label, size_t size)
{
	ptrdiff_t i;

	for (i = size - 1; i >= 0; i--) {
		if (label[i] == '\0')
			continue;
		else if (label[i] == ' ')
			label[i] = '\0';
		else
			break;
	}
}

static void
usage(void)
{

	fprintf(stderr, "usage: fstyp [-l] [-s] [-u] special\n");
	exit(1);
}

static void
type_check(const char *path, FILE *fp)
{
	int error, fd;
	off_t mediasize;
	struct stat sb;

	fd = fileno(fp);

	error = fstat(fd, &sb);
	if (error != 0)
		err(1, "%s: fstat", path);

	if (S_ISREG(sb.st_mode))
		return;

	error = ioctl(fd, DIOCGMEDIASIZE, &mediasize);
	if (error != 0)
		errx(1, "%s: not a disk", path);
}

int
main(int argc, char **argv)
{
	int ch, error, i, nbytes;
	bool ignore_type = false, show_unmountable = false;
	char label[LABEL_LEN + 1], strvised[LABEL_LEN * 4 + 1];
	char *path;
	FILE *fp;
	fstyp_function fstyp_f;

	while ((ch = getopt(argc, argv, "lsu")) != -1) {
		switch (ch) {
		case 'l':
			show_label = true;
			break;
		case 's':
			ignore_type = true;
			break;
		case 'u':
			show_unmountable = true;
			break;
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;
	if (argc != 1)
		usage();

	path = argv[0];

	if (setlocale(LC_CTYPE, "") == NULL)
		err(1, "setlocale");
	caph_cache_catpages();

#ifdef WITH_ICONV
	/* Cache iconv conversion data before entering capability mode. */
	if (show_label) {
		for (i = 0; i < (int)nitems(fstypes); i++) {
			iconv_t cd;

			if (fstypes[i].precache_encoding == NULL)
				continue;
			cd = iconv_open("", fstypes[i].precache_encoding);
			if (cd == (iconv_t)-1)
				err(1, "%s: iconv_open %s", fstypes[i].name,
				    fstypes[i].precache_encoding);
			/* Iconv keeps a small cache of unused encodings. */
			iconv_close(cd);
		}
	}
#endif

	fp = fopen(path, "r");
	if (fp == NULL)
		err(1, "%s", path);

	if (caph_enter() < 0)
		err(1, "cap_enter");

	if (ignore_type == false)
		type_check(path, fp);

	memset(label, '\0', sizeof(label));

	for (i = 0;; i++) {
		if (show_unmountable == false && fstypes[i].unmountable == true)
			continue;
		fstyp_f = fstypes[i].function;
		if (fstyp_f == NULL)
			break;

		error = fstyp_f(fp, label, sizeof(label));
		if (error == 0)
			break;
	}

	if (fstypes[i].name == NULL) {
		warnx("%s: filesystem not recognized", path);
		return (1);
	}

	if (show_label && label[0] != '\0') {
		/*
		 * XXX: I'd prefer VIS_HTTPSTYLE, but it unconditionally
		 *      encodes spaces.
		 */
		nbytes = strsnvis(strvised, sizeof(strvised), label,
		    VIS_GLOB | VIS_NL, "\"'$");
		if (nbytes == -1)
			err(1, "strsnvis");

		printf("%s %s\n", fstypes[i].name, strvised);
	} else {
		printf("%s\n", fstypes[i].name);
	}

	return (0);
}
