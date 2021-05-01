/*-
 * Copyright (c) 2014 The FreeBSD Foundation
 * All rights reserved.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/disk.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

#include <err.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef WITH_ICONV
#include <iconv.h>
#endif

#include "fstyp.h"
#include "fstyp_p.h"

bool encodings_enabled = false;

int	fstyp_apfs(FILE *fp, char *label, size_t size);
int	fstyp_cd9660(FILE *fp, char *label, size_t size);
int	fstyp_exfat(FILE *fp, char *label, size_t size);
int	fstyp_ext2fs(FILE *fp, char *label, size_t size);
int	fstyp_geli(FILE *fp, char *label, size_t size);
int	fstyp_hammer(FILE *fp, char *label, size_t size);
int	fstyp_hammer2(FILE *fp, char *label, size_t size);
int	fstyp_hfsp(FILE *fp, char *label, size_t size);
int	fstyp_msdosfs(FILE *fp, char *label, size_t size);
int	fstyp_ntfs(FILE *fp, char *label, size_t size);
int	fstyp_ufs(FILE *fp, char *label, size_t size);
#ifdef HAVE_ZFS
int	fstyp_zfs(FILE *fp, char *label, size_t size);
#endif

static const struct fstype fstypes[] = {
	{ "apfs", &fstyp_apfs, true, NULL },
	{ "cd9660", &fstyp_cd9660, false, NULL },
	{ "exfat", &fstyp_exfat, false, EXFAT_ENC },
	{ "ext2fs", &fstyp_ext2fs, false, NULL },
	{ "geli", &fstyp_geli, true, NULL },
	{ "hammer", &fstyp_hammer, true, NULL },
	{ "hammer2", &fstyp_hammer2, true, NULL },
	{ "hfs+", &fstyp_hfsp, false, NULL },
	{ "msdosfs", &fstyp_msdosfs, false, NULL },
	{ "ntfs", &fstyp_ntfs, false, NTFS_ENC },
	{ "ufs", &fstyp_ufs, false, NULL },
#ifdef HAVE_ZFS
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

void
enable_encodings()
{
	encodings_enabled = true;

#ifdef WITH_ICONV
	int i;

	/* Cache iconv conversion data before entering capability mode. */
	
	for (i = 0; i < nitems(fstypes); i++) {
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
#endif
}

int 
fstypef(FILE *fp, char *label, size_t label_len, bool show_unmountable, const struct fstype **result)
{
	int error, i;
	fstyp_function fstyp_f;

	*result = NULL;
	error = -1;

	for (i = 0;; i++) {
		if (show_unmountable == false && fstypes[i].unmountable == true)
			continue;

		*result = &fstypes[i];	
		fstyp_f = fstypes[i].function;
		if (fstyp_f == NULL)
			return -1;

		error = fstyp_f(fp, label, label_len);
		if (error == 0)
			break;
	}

	return error;
}