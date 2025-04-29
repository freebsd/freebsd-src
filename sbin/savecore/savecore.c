/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
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
 * 3. Neither the name of the University nor the names of its contributors
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

#include <sys/param.h>
#include <sys/disk.h>
#include <sys/kerneldump.h>
#include <sys/memrange.h>
#include <sys/mount.h>
#include <sys/stat.h>

#include <capsicum_helpers.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <fstab.h>
#include <paths.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>
#define	Z_SOLO
#include <zlib.h>
#include <zstd.h>

#include <libcasper.h>
#include <casper/cap_fileargs.h>
#include <casper/cap_syslog.h>

#include <libxo/xo.h>

/* The size of the buffer used for I/O. */
#define	BUFFERSIZE	(1024*1024)

#define	STATUS_BAD	0
#define	STATUS_GOOD	1
#define	STATUS_UNKNOWN	2

#define LOG_OPTIONS  LOG_PERROR
#define LOG_FACILITY LOG_DAEMON

static cap_channel_t *capsyslog;
static fileargs_t *capfa;
static bool checkfor, compress, uncompress, clear, force, keep;	/* flags */
static bool livecore;	/* flags cont. */
static int verbose;
static int nfound, nsaved, nerr;			/* statistics */
static int maxdumps;
static uint8_t comp_desired;

extern FILE *zdopen(int, const char *);

static sig_atomic_t got_siginfo;
static void infohandler(int);

static void
logmsg(int pri, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	if (capsyslog != NULL)
		cap_vsyslog(capsyslog, pri, fmt, ap);
	else
		vsyslog(pri, fmt, ap);
	va_end(ap);
}

static FILE *
xfopenat(int dirfd, const char *path, int flags, const char *modestr, ...)
{
	va_list ap;
	FILE *fp;
	mode_t mode;
	int error, fd;

	if ((flags & O_CREAT) == O_CREAT) {
		va_start(ap, modestr);
		mode = (mode_t)va_arg(ap, int);
		va_end(ap);
	} else
		mode = 0;

	fd = openat(dirfd, path, flags, mode);
	if (fd < 0)
		return (NULL);
	fp = fdopen(fd, modestr);
	if (fp == NULL) {
		error = errno;
		(void)close(fd);
		errno = error;
	}
	return (fp);
}

static void
printheader(xo_handle_t *xo, const struct kerneldumpheader *h,
    const char *device, int bounds, const int status)
{
	uint64_t dumplen;
	time_t t;
	struct tm tm;
	char time_str[64];
	const char *stat_str;
	const char *comp_str;

	xo_flush_h(xo);
	xo_emit_h(xo, "{Lwc:Dump header from device}{:dump_device/%s}\n",
	    device);
	xo_emit_h(xo, "{P:  }{Lwc:Architecture}{:architecture/%s}\n",
	    h->architecture);
	xo_emit_h(xo,
	    "{P:  }{Lwc:Architecture Version}{:architecture_version/%u}\n",
	    dtoh32(h->architectureversion));
	dumplen = dtoh64(h->dumplength);
	xo_emit_h(xo, "{P:  }{Lwc:Dump Length}{:dump_length_bytes/%lld}\n",
	    (long long)dumplen);
	xo_emit_h(xo, "{P:  }{Lwc:Blocksize}{:blocksize/%d}\n",
	    dtoh32(h->blocksize));
	switch (h->compression) {
	case KERNELDUMP_COMP_NONE:
		comp_str = "none";
		break;
	case KERNELDUMP_COMP_GZIP:
		comp_str = "gzip";
		break;
	case KERNELDUMP_COMP_ZSTD:
		comp_str = "zstd";
		break;
	default:
		comp_str = "???";
		break;
	}
	xo_emit_h(xo, "{P:  }{Lwc:Compression}{:compression/%s}\n", comp_str);
	t = dtoh64(h->dumptime);
	localtime_r(&t, &tm);
	if (strftime(time_str, sizeof(time_str), "%F %T %z", &tm) == 0)
		time_str[0] = '\0';
	xo_emit_h(xo, "{P:  }{Lwc:Dumptime}{:dumptime/%s}\n", time_str);
	xo_emit_h(xo, "{P:  }{Lwc:Hostname}{:hostname/%s}\n", h->hostname);
	xo_emit_h(xo, "{P:  }{Lwc:Magic}{:magic/%s}\n", h->magic);
	xo_emit_h(xo, "{P:  }{Lwc:Version String}{:version_string/%s}",
	    h->versionstring);
	xo_emit_h(xo, "{P:  }{Lwc:Panic String}{:panic_string/%s}\n",
	    h->panicstring);
	xo_emit_h(xo, "{P:  }{Lwc:Dump Parity}{:dump_parity/%u}\n", h->parity);
	xo_emit_h(xo, "{P:  }{Lwc:Bounds}{:bounds/%d}\n", bounds);

	switch (status) {
	case STATUS_BAD:
		stat_str = "bad";
		break;
	case STATUS_GOOD:
		stat_str = "good";
		break;
	default:
		stat_str = "unknown";
		break;
	}
	xo_emit_h(xo, "{P:  }{Lwc:Dump Status}{:dump_status/%s}\n", stat_str);
	xo_flush_h(xo);
}

static int
getbounds(int savedirfd)
{
	FILE *fp;
	char buf[6];
	int ret;

	/*
	 * If we are just checking, then we haven't done a chdir to the dump
	 * directory and we should not try to read a bounds file.
	 */
	if (checkfor)
		return (0);

	ret = 0;

	if ((fp = xfopenat(savedirfd, "bounds", O_RDONLY, "r")) == NULL) {
		if (verbose)
			printf("unable to open bounds file, using 0\n");
		return (ret);
	}
	if (fgets(buf, sizeof(buf), fp) == NULL) {
		if (feof(fp))
			logmsg(LOG_WARNING, "bounds file is empty, using 0");
		else
			logmsg(LOG_WARNING, "bounds file: %s", strerror(errno));
		fclose(fp);
		return (ret);
	}

	errno = 0;
	ret = (int)strtol(buf, NULL, 10);
	if (ret == 0 && (errno == EINVAL || errno == ERANGE))
		logmsg(LOG_WARNING, "invalid value found in bounds, using 0");
	if (maxdumps > 0 && ret == maxdumps)
		ret = 0;
	fclose(fp);
	return (ret);
}

static void
writebounds(int savedirfd, int bounds)
{
	FILE *fp;

	if ((fp = xfopenat(savedirfd, "bounds", O_WRONLY | O_CREAT | O_TRUNC,
	    "w", 0644)) == NULL) {
		logmsg(LOG_WARNING, "unable to write to bounds file: %m");
		return;
	}

	if (verbose)
		printf("bounds number: %d\n", bounds);

	fprintf(fp, "%d\n", bounds);
	fclose(fp);
}

static bool
writekey(int savedirfd, const char *keyname, uint8_t *dumpkey,
    uint32_t dumpkeysize)
{
	int fd;

	fd = openat(savedirfd, keyname, O_WRONLY | O_CREAT | O_TRUNC, 0600);
	if (fd == -1) {
		logmsg(LOG_ERR, "Unable to open %s to write the key: %m.",
		    keyname);
		return (false);
	}

	if (write(fd, dumpkey, dumpkeysize) != (ssize_t)dumpkeysize) {
		logmsg(LOG_ERR, "Unable to write the key to %s: %m.", keyname);
		close(fd);
		return (false);
	}

	close(fd);
	return (true);
}

static int
write_header_info(xo_handle_t *xostdout, const struct kerneldumpheader *kdh,
    int savedirfd, const char *infoname, const char *device, int bounds,
    int status)
{
	xo_handle_t *xoinfo;
	FILE *info;

	/*
	 * Create or overwrite any existing dump header files.
	 */
	if ((info = xfopenat(savedirfd, infoname,
	    O_WRONLY | O_CREAT | O_TRUNC, "w", 0600)) == NULL) {
		logmsg(LOG_ERR, "open(%s): %m", infoname);
		return (-1);
	}

	xoinfo = xo_create_to_file(info, xo_get_style(NULL), 0);
	if (xoinfo == NULL) {
		logmsg(LOG_ERR, "%s: %m", infoname);
		fclose(info);
		return (-1);
	}
	xo_open_container_h(xoinfo, "crashdump");

	if (verbose)
		printheader(xostdout, kdh, device, bounds, status);

	printheader(xoinfo, kdh, device, bounds, status);
	xo_close_container_h(xoinfo, "crashdump");
	xo_flush_h(xoinfo);
	if (xo_finish_h(xoinfo) < 0)
		xo_err(EXIT_FAILURE, "stdout");
	fclose(info);

	return (0);
}

static off_t
file_size(int savedirfd, const char *path)
{
	struct stat sb;

	/* Ignore all errors, this file may not exist. */
	if (fstatat(savedirfd, path, &sb, 0) == -1)
		return (0);
	return (sb.st_size);
}

static off_t
saved_dump_size(int savedirfd, int bounds)
{
	char path[32];
	off_t dumpsize;

	dumpsize = 0;

	(void)snprintf(path, sizeof(path), "info.%d", bounds);
	dumpsize += file_size(savedirfd, path);
	(void)snprintf(path, sizeof(path), "vmcore.%d", bounds);
	dumpsize += file_size(savedirfd, path);
	(void)snprintf(path, sizeof(path), "vmcore.%d.gz", bounds);
	dumpsize += file_size(savedirfd, path);
	(void)snprintf(path, sizeof(path), "vmcore.%d.zst", bounds);
	dumpsize += file_size(savedirfd, path);
	(void)snprintf(path, sizeof(path), "textdump.tar.%d", bounds);
	dumpsize += file_size(savedirfd, path);
	(void)snprintf(path, sizeof(path), "textdump.tar.%d.gz", bounds);
	dumpsize += file_size(savedirfd, path);

	return (dumpsize);
}

static void
saved_dump_remove(int savedirfd, int bounds)
{
	char path[32];

	(void)snprintf(path, sizeof(path), "info.%d", bounds);
	(void)unlinkat(savedirfd, path, 0);
	(void)snprintf(path, sizeof(path), "vmcore.%d", bounds);
	(void)unlinkat(savedirfd, path, 0);
	(void)snprintf(path, sizeof(path), "vmcore.%d.gz", bounds);
	(void)unlinkat(savedirfd, path, 0);
	(void)snprintf(path, sizeof(path), "vmcore.%d.zst", bounds);
	(void)unlinkat(savedirfd, path, 0);
	(void)snprintf(path, sizeof(path), "textdump.tar.%d", bounds);
	(void)unlinkat(savedirfd, path, 0);
	(void)snprintf(path, sizeof(path), "textdump.tar.%d.gz", bounds);
	(void)unlinkat(savedirfd, path, 0);
	(void)snprintf(path, sizeof(path), "livecore.%d", bounds);
	(void)unlinkat(savedirfd, path, 0);
	(void)snprintf(path, sizeof(path), "livecore.%d.gz", bounds);
	(void)unlinkat(savedirfd, path, 0);
	(void)snprintf(path, sizeof(path), "livecore.%d.zst", bounds);
	(void)unlinkat(savedirfd, path, 0);
}

static void
symlinks_remove(int savedirfd)
{

	(void)unlinkat(savedirfd, "info.last", 0);
	(void)unlinkat(savedirfd, "key.last", 0);
	(void)unlinkat(savedirfd, "vmcore.last", 0);
	(void)unlinkat(savedirfd, "vmcore.last.gz", 0);
	(void)unlinkat(savedirfd, "vmcore.last.zst", 0);
	(void)unlinkat(savedirfd, "vmcore_encrypted.last", 0);
	(void)unlinkat(savedirfd, "vmcore_encrypted.last.gz", 0);
	(void)unlinkat(savedirfd, "textdump.tar.last", 0);
	(void)unlinkat(savedirfd, "textdump.tar.last.gz", 0);
	(void)unlinkat(savedirfd, "livecore.last", 0);
	(void)unlinkat(savedirfd, "livecore.last.gz", 0);
	(void)unlinkat(savedirfd, "livecore.last.zst", 0);
}

/*
 * Check that sufficient space is available on the disk that holds the
 * save directory.
 */
static int
check_space(const char *savedir, int savedirfd, off_t dumpsize, int bounds)
{
	char buf[100];
	struct statfs fsbuf;
	FILE *fp;
	off_t available, minfree, spacefree, totfree, needed;

	if (fstatfs(savedirfd, &fsbuf) < 0) {
		logmsg(LOG_ERR, "%s: %m", savedir);
		exit(EXIT_FAILURE);
	}
	spacefree = ((off_t) fsbuf.f_bavail * fsbuf.f_bsize) / 1024;
	totfree = ((off_t) fsbuf.f_bfree * fsbuf.f_bsize) / 1024;

	if ((fp = xfopenat(savedirfd, "minfree", O_RDONLY, "r")) == NULL)
		minfree = 0;
	else {
		if (fgets(buf, sizeof(buf), fp) == NULL)
			minfree = 0;
		else {
			char *endp;

			errno = 0;
			minfree = strtoll(buf, &endp, 10);
			if (minfree == 0 && errno != 0)
				minfree = -1;
			else {
				while (*endp != '\0' && isspace(*endp))
					endp++;
				if (*endp != '\0' || minfree < 0)
					minfree = -1;
			}
			if (minfree < 0)
				logmsg(LOG_WARNING,
				    "`minfree` didn't contain a valid size "
				    "(`%s`). Defaulting to 0", buf);
		}
		(void)fclose(fp);
	}

	available = minfree > 0 ? spacefree - minfree : totfree;
	needed = dumpsize / 1024 + 2;	/* 2 for info file */
	needed -= saved_dump_size(savedirfd, bounds);
	if (available < needed) {
		logmsg(LOG_WARNING,
		    "no dump: not enough free space on device (need at least "
		    "%jdkB for dump; %jdkB available; %jdkB reserved)",
		    (intmax_t)needed,
		    (intmax_t)available + minfree,
		    (intmax_t)minfree);
		return (0);
	}
	if (spacefree - needed < 0)
		logmsg(LOG_WARNING,
		    "dump performed, but free space threshold crossed");
	return (1);
}

static bool
compare_magic(const struct kerneldumpheader *kdh, const char *magic)
{

	return (strncmp(kdh->magic, magic, sizeof(kdh->magic)) == 0);
}

#define BLOCKSIZE (1<<12)
#define BLOCKMASK (~(BLOCKSIZE-1))

static size_t
sparsefwrite(const char *buf, size_t nr, FILE *fp)
{
	size_t nw, he, hs;

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
		 * If hs > nw, buf[nw..hs] contains non-zero
		 * data. If he > hs, buf[hs..he] is all zeroes.
		 */
		if (hs > nw)
			if (fwrite(buf + nw, hs - nw, 1, fp) != 1)
				break;
		if (he > hs)
			if (fseeko(fp, he - hs, SEEK_CUR) == -1)
				break;
	}

	return (nw);
}

static char *zbuf;
static size_t zbufsize;

static ssize_t
GunzipWrite(z_stream *z, char *in, size_t insize, FILE *fp)
{
	static bool firstblock = true;		/* XXX not re-entrable/usable */
	const size_t hdrlen = 10;
	size_t nw = 0, w;
	int rv;

	z->next_in = in;
	z->avail_in = insize;
	/*
	 * Since contrib/zlib for some reason is compiled
	 * without GUNZIP define, we need to skip the gzip
	 * header manually.  Kernel puts minimal 10 byte
	 * header, see sys/kern/subr_compressor.c:gz_reset().
	 */
	if (firstblock) {
		z->next_in += hdrlen;
		z->avail_in -= hdrlen;
		firstblock = false;
	}
	do {
		z->next_out = zbuf;
		z->avail_out = zbufsize;
		rv = inflate(z, Z_NO_FLUSH);
		if (rv != Z_OK && rv != Z_STREAM_END) {
			logmsg(LOG_ERR, "decompression failed: %s", z->msg);
			return (-1);
		}
		w = sparsefwrite(zbuf, zbufsize - z->avail_out, fp);
		if (w < zbufsize - z->avail_out)
			return (-1);
		nw += w;
	} while (z->avail_in > 0 && rv != Z_STREAM_END);

	return (nw);
}

static ssize_t
ZstdWrite(ZSTD_DCtx *Zctx, char *in, size_t insize, FILE *fp)
{
	ZSTD_inBuffer Zin;
	ZSTD_outBuffer Zout;
	size_t nw = 0, w;
	int rv;

	Zin.src = in;
	Zin.size = insize;
	Zin.pos = 0;
	do {
		Zout.dst = zbuf;
		Zout.size = zbufsize;
		Zout.pos = 0;
		rv = ZSTD_decompressStream(Zctx, &Zout, &Zin);
		if (ZSTD_isError(rv)) {
			logmsg(LOG_ERR, "decompression failed: %s",
			    ZSTD_getErrorName(rv));
			return (-1);
		}
		w = sparsefwrite(zbuf, Zout.pos, fp);
		if (w < Zout.pos)
			return (-1);
		nw += w;
	} while (Zin.pos < Zin.size && rv != 0);

	return (nw);
}

static int
DoRegularFile(int fd, off_t dumpsize, u_int sectorsize, bool sparse,
    uint8_t compression, char *buf, const char *device,
    const char *filename, FILE *fp)
{
	size_t nr, wl;
	ssize_t nw;
	off_t dmpcnt, origsize;
	z_stream z;		/* gzip */
	ZSTD_DCtx *Zctx;	/* zstd */

	dmpcnt = 0;
	origsize = dumpsize;
	if (compression == KERNELDUMP_COMP_GZIP) {
		memset(&z, 0, sizeof(z));
		z.zalloc = Z_NULL;
		z.zfree = Z_NULL;
		if (inflateInit2(&z, -MAX_WBITS) != Z_OK) {
			logmsg(LOG_ERR, "failed to initialize zlib: %s", z.msg);
			return (-1);
		}
		zbufsize = BUFFERSIZE;
	} else if (compression == KERNELDUMP_COMP_ZSTD) {
		if ((Zctx = ZSTD_createDCtx()) == NULL) {
			logmsg(LOG_ERR, "failed to initialize zstd");
			return (-1);
		}
		zbufsize = ZSTD_DStreamOutSize();
	}
	if (zbufsize > 0)
		if ((zbuf = malloc(zbufsize)) == NULL) {
			logmsg(LOG_ERR, "failed to alloc decompression buffer");
			return (-1);
		}

	while (dumpsize > 0) {
		wl = BUFFERSIZE;
		if (wl > (size_t)dumpsize)
			wl = dumpsize;
		nr = read(fd, buf, roundup(wl, sectorsize));
		if (nr != roundup(wl, sectorsize)) {
			if (nr == 0)
				logmsg(LOG_WARNING,
				    "WARNING: EOF on dump device");
			else
				logmsg(LOG_ERR, "read error on %s: %m", device);
			nerr++;
			return (-1);
		}
		if (compression == KERNELDUMP_COMP_GZIP)
			nw = GunzipWrite(&z, buf, nr, fp);
		else if (compression == KERNELDUMP_COMP_ZSTD)
			nw = ZstdWrite(Zctx, buf, nr, fp);
		else if (!sparse)
			nw = fwrite(buf, 1, wl, fp);
		else
			nw = sparsefwrite(buf, wl, fp);
		if (nw < 0 || (compression == KERNELDUMP_COMP_NONE &&
		     (size_t)nw != wl)) {
			logmsg(LOG_ERR,
			    "write error on %s file: %m", filename);
			logmsg(LOG_WARNING,
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
		logmsg(LOG_ERR, "textdump uneven multiple of 512 on %s",
		    device);
		nerr++;
		return (-1);
	}
	while (dumpsize > 0) {
		nr = pread(fd, buf, wl, lasthd - (totsize - dumpsize) - wl);
		if (nr != wl) {
			if (nr == 0)
				logmsg(LOG_WARNING,
				    "WARNING: EOF on dump device");
			else
				logmsg(LOG_ERR, "read error on %s: %m", device);
			nerr++;
			return (-1);
		}
		nw = fwrite(buf, 1, wl, fp);
		if (nw != wl) {
			logmsg(LOG_ERR,
			    "write error on %s file: %m", filename);
			logmsg(LOG_WARNING,
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
DoLiveFile(const char *savedir, int savedirfd, const char *device)
{
	char infoname[32], corename[32], linkname[32], tmpname[32];
	struct mem_livedump_arg marg;
	struct kerneldumpheader kdhl;
	xo_handle_t *xostdout;
	off_t dumplength;
	uint32_t version;
	int fddev, fdcore;
	int bounds;
	int error, status;

	bounds = getbounds(savedirfd);
	status = STATUS_UNKNOWN;

	xostdout = xo_create_to_file(stdout, XO_STYLE_TEXT, 0);
	if (xostdout == NULL) {
		logmsg(LOG_ERR, "xo_create_to_file() failed: %m");
		return;
	}

	/*
	 * Create a temporary file. We will invoke the live dump and its
	 * contents will be written to this fd. After validating and removing
	 * the kernel dump header from the tail-end of this file, it will be
	 * renamed to its definitive filename (e.g. livecore.2.gz).
	 *
	 * If any errors are encountered before the rename, the temporary file
	 * is unlinked.
	 */
	strcpy(tmpname, "livecore.tmp.XXXXXX");
	fdcore = mkostempsat(savedirfd, tmpname, 0, 0);
	if (fdcore < 0) {
		logmsg(LOG_ERR, "error opening temp file: %m");
		return;
	}

	fddev = fileargs_open(capfa, device);
	if (fddev < 0) {
		logmsg(LOG_ERR, "%s: %m", device);
		goto unlinkexit;
	}

	bzero(&marg, sizeof(marg));
	marg.fd = fdcore;
	marg.compression = comp_desired;
	if (ioctl(fddev, MEM_KERNELDUMP, &marg) == -1) {
		logmsg(LOG_ERR,
		    "failed to invoke live-dump on system: %m");
		close(fddev);
		goto unlinkexit;
	}

	/* Close /dev/mem fd, we are finished with it. */
	close(fddev);

	/* Seek to the end of the file, minus the size of the header. */
	if (lseek(fdcore, -(off_t)sizeof(kdhl), SEEK_END) == -1) {
		logmsg(LOG_ERR, "failed to lseek: %m");
		goto unlinkexit;
	}

	if (read(fdcore, &kdhl, sizeof(kdhl)) != sizeof(kdhl)) {
		logmsg(LOG_ERR, "failed to read kernel dump header: %m");
		goto unlinkexit;
	}
	/* Reset cursor */
	(void)lseek(fdcore, 0, SEEK_SET);

	/* Validate the dump header. */
	version = dtoh32(kdhl.version);
	if (compare_magic(&kdhl, KERNELDUMPMAGIC)) {
		if (version != KERNELDUMPVERSION) {
			logmsg(LOG_ERR,
			    "unknown version (%d) in dump header on %s",
			    version, device);
			goto unlinkexit;
		} else if (kdhl.compression != comp_desired) {
			/* This should be impossible. */
			logmsg(LOG_ERR,
			    "dump compression (%u) doesn't match request (%u)",
			    kdhl.compression, comp_desired);
			if (!force)
				goto unlinkexit;
		}
	} else {
		logmsg(LOG_ERR, "magic mismatch on live dump header");
		goto unlinkexit;
	}
	if (kerneldump_parity(&kdhl)) {
		logmsg(LOG_ERR,
		    "parity error on last dump header on %s", device);
		nerr++;
		status = STATUS_BAD;
		if (!force)
			goto unlinkexit;
	} else {
		status = STATUS_GOOD;
	}

	nfound++;
	dumplength = dtoh64(kdhl.dumplength);
	if (dtoh32(kdhl.dumpkeysize) != 0) {
		logmsg(LOG_ERR,
		    "dump header unexpectedly reported keysize > 0");
		goto unlinkexit;
	}

	/* Remove the vestigial kernel dump header. */
	error = ftruncate(fdcore, dumplength);
	if (error != 0) {
		logmsg(LOG_ERR, "failed to truncate the core file: %m");
		goto unlinkexit;
	}

	if (verbose >= 2) {
		printf("\nDump header:\n");
		printheader(xostdout, &kdhl, device, bounds, -1);
		printf("\n");
	}
	logmsg(LOG_ALERT, "livedump");

	writebounds(savedirfd, bounds + 1);
	saved_dump_remove(savedirfd, bounds);

	snprintf(corename, sizeof(corename), "livecore.%d", bounds);
	if (compress)
		strcat(corename, kdhl.compression == KERNELDUMP_COMP_ZSTD ?
		    ".zst" : ".gz");

	if (verbose)
		printf("renaming %s to %s\n", tmpname, corename);
	if (renameat(savedirfd, tmpname, savedirfd, corename) != 0) {
		logmsg(LOG_ERR, "renameat failed: %m");
		goto unlinkexit;
	}

	snprintf(infoname, sizeof(infoname), "info.%d", bounds);
	if (write_header_info(xostdout, &kdhl, savedirfd, infoname, device,
	    bounds, status) != 0) {
		nerr++;
		return;
	}

	logmsg(LOG_NOTICE, "writing %score to %s/%s",
	    compress ? "compressed " : "", savedir, corename);

	if (verbose)
		printf("\n");

	symlinks_remove(savedirfd);
	if (symlinkat(infoname, savedirfd, "info.last") == -1) {
		logmsg(LOG_WARNING, "unable to create symlink %s/%s: %m",
		    savedir, "info.last");
	}

	snprintf(linkname, sizeof(linkname), "livecore.last");
	if (compress)
		strcat(linkname, kdhl.compression == KERNELDUMP_COMP_ZSTD ?
		    ".zst" : ".gz");
	if (symlinkat(corename, savedirfd, linkname) == -1) {
		logmsg(LOG_WARNING, "unable to create symlink %s/%s: %m",
		    savedir, linkname);
	}

	nsaved++;
	if (verbose)
		printf("dump saved\n");

	close(fdcore);
	return;
unlinkexit:
	funlinkat(savedirfd, tmpname, fdcore, 0);
	close(fdcore);
}

static void
DoFile(const char *savedir, int savedirfd, const char *device)
{
	static char *buf = NULL;
	xo_handle_t *xostdout;
	char infoname[32], corename[32], linkname[32], keyname[32];
	char *temp = NULL;
	struct kerneldumpheader kdhf, kdhl;
	uint8_t *dumpkey;
	off_t mediasize, dumpextent, dumplength, firsthd, lasthd;
	FILE *core;
	int fdcore, fddev, error;
	int bounds, status;
	u_int sectorsize;
	uint32_t dumpkeysize;
	bool iscompressed, isencrypted, istextdump, ret;

	/* Live kernel dumps are handled separately. */
	if (livecore) {
		DoLiveFile(savedir, savedirfd, device);
		return;
	}

	bounds = getbounds(savedirfd);
	dumpkey = NULL;
	mediasize = 0;
	status = STATUS_UNKNOWN;

	xostdout = xo_create_to_file(stdout, XO_STYLE_TEXT, 0);
	if (xostdout == NULL) {
		logmsg(LOG_ERR, "xo_create_to_file() failed: %m");
		return;
	}

	if (buf == NULL) {
		buf = malloc(BUFFERSIZE);
		if (buf == NULL) {
			logmsg(LOG_ERR, "%m");
			return;
		}
	}

	if (verbose)
		printf("checking for kernel dump on device %s\n", device);

	fddev = fileargs_open(capfa, device);
	if (fddev < 0) {
		logmsg(LOG_ERR, "%s: %m", device);
		return;
	}

	error = ioctl(fddev, DIOCGMEDIASIZE, &mediasize);
	if (!error)
		error = ioctl(fddev, DIOCGSECTORSIZE, &sectorsize);
	if (error) {
		logmsg(LOG_ERR,
		    "couldn't find media and/or sector size of %s: %m", device);
		goto closefd;
	}

	if (verbose) {
		printf("mediasize = %lld bytes\n", (long long)mediasize);
		printf("sectorsize = %u bytes\n", sectorsize);
	}

	if (sectorsize < sizeof(kdhl)) {
		logmsg(LOG_ERR,
		    "Sector size is less the kernel dump header %zu",
		    sizeof(kdhl));
		goto closefd;
	}

	lasthd = mediasize - sectorsize;
	temp = malloc(sectorsize);
	if (temp == NULL) {
		logmsg(LOG_ERR, "%m");
		goto closefd;
	}
	if (lseek(fddev, lasthd, SEEK_SET) != lasthd ||
	    read(fddev, temp, sectorsize) != (ssize_t)sectorsize) {
		logmsg(LOG_ERR,
		    "error reading last dump header at offset %lld in %s: %m",
		    (long long)lasthd, device);
		goto closefd;
	}
	memcpy(&kdhl, temp, sizeof(kdhl));
	iscompressed = istextdump = false;
	if (compare_magic(&kdhl, TEXTDUMPMAGIC)) {
		if (verbose)
			printf("textdump magic on last dump header on %s\n",
			    device);
		istextdump = true;
		if (dtoh32(kdhl.version) != KERNELDUMP_TEXT_VERSION) {
			logmsg(LOG_ERR,
			    "unknown version (%d) in last dump header on %s",
			    dtoh32(kdhl.version), device);

			status = STATUS_BAD;
			if (!force)
				goto closefd;
		}
	} else if (compare_magic(&kdhl, KERNELDUMPMAGIC)) {
		if (dtoh32(kdhl.version) != KERNELDUMPVERSION) {
			logmsg(LOG_ERR,
			    "unknown version (%d) in last dump header on %s",
			    dtoh32(kdhl.version), device);

			status = STATUS_BAD;
			if (!force)
				goto closefd;
		}
		switch (kdhl.compression) {
		case KERNELDUMP_COMP_NONE:
			uncompress = false;
			break;
		case KERNELDUMP_COMP_GZIP:
		case KERNELDUMP_COMP_ZSTD:
			if (compress && verbose)
				printf("dump is already compressed\n");
			if (uncompress && verbose)
				printf("dump to be uncompressed\n");
			compress = false;
			iscompressed = true;
			break;
		default:
			logmsg(LOG_ERR, "unknown compression type %d on %s",
			    kdhl.compression, device);
			break;
		}
	} else {
		if (verbose)
			printf("magic mismatch on last dump header on %s\n",
			    device);

		status = STATUS_BAD;
		if (!force)
			goto closefd;

		if (compare_magic(&kdhl, KERNELDUMPMAGIC_CLEARED)) {
			if (verbose)
				printf("forcing magic on %s\n", device);
			memcpy(kdhl.magic, KERNELDUMPMAGIC, sizeof(kdhl.magic));
		} else {
			logmsg(LOG_ERR, "unable to force dump - bad magic");
			goto closefd;
		}
		if (dtoh32(kdhl.version) != KERNELDUMPVERSION) {
			logmsg(LOG_ERR,
			    "unknown version (%d) in last dump header on %s",
			    dtoh32(kdhl.version), device);

			status = STATUS_BAD;
			if (!force)
				goto closefd;
		}
	}

	nfound++;
	if (clear)
		goto nuke;

	if (kerneldump_parity(&kdhl)) {
		logmsg(LOG_ERR,
		    "parity error on last dump header on %s", device);
		nerr++;
		status = STATUS_BAD;
		if (!force)
			goto closefd;
	}
	dumpextent = dtoh64(kdhl.dumpextent);
	dumplength = dtoh64(kdhl.dumplength);
	dumpkeysize = dtoh32(kdhl.dumpkeysize);
	firsthd = lasthd - dumpextent - sectorsize - dumpkeysize;
	if (lseek(fddev, firsthd, SEEK_SET) != firsthd ||
	    read(fddev, temp, sectorsize) != (ssize_t)sectorsize) {
		logmsg(LOG_ERR,
		    "error reading first dump header at offset %lld in %s: %m",
		    (long long)firsthd, device);
		nerr++;
		goto closefd;
	}
	memcpy(&kdhf, temp, sizeof(kdhf));

	if (verbose >= 2) {
		printf("First dump headers:\n");
		printheader(xostdout, &kdhf, device, bounds, -1);

		printf("\nLast dump headers:\n");
		printheader(xostdout, &kdhl, device, bounds, -1);
		printf("\n");
	}

	if (memcmp(&kdhl, &kdhf, sizeof(kdhl))) {
		logmsg(LOG_ERR,
		    "first and last dump headers disagree on %s", device);
		nerr++;
		status = STATUS_BAD;
		if (!force)
			goto closefd;
	} else {
		status = STATUS_GOOD;
	}

	if (checkfor) {
		printf("A dump exists on %s\n", device);
		close(fddev);
		exit(EXIT_SUCCESS);
	}

	if (kdhl.panicstring[0] != '\0')
		logmsg(LOG_ALERT, "reboot after panic: %.*s",
		    (int)sizeof(kdhl.panicstring), kdhl.panicstring);
	else
		logmsg(LOG_ALERT, "reboot");

	if (verbose)
		printf("Checking for available free space\n");

	if (!check_space(savedir, savedirfd, dumplength, bounds)) {
		nerr++;
		goto closefd;
	}

	writebounds(savedirfd, bounds + 1);

	saved_dump_remove(savedirfd, bounds);

	isencrypted = (dumpkeysize > 0);
	if (compress)
		snprintf(corename, sizeof(corename), "%s.%d.gz",
		    istextdump ? "textdump.tar" :
		    (isencrypted ? "vmcore_encrypted" : "vmcore"), bounds);
	else if (iscompressed && !isencrypted && !uncompress)
		snprintf(corename, sizeof(corename), "vmcore.%d.%s", bounds,
		    (kdhl.compression == KERNELDUMP_COMP_GZIP) ? "gz" : "zst");
	else
		snprintf(corename, sizeof(corename), "%s.%d",
		    istextdump ? "textdump.tar" :
		    (isencrypted ? "vmcore_encrypted" : "vmcore"), bounds);
	fdcore = openat(savedirfd, corename, O_WRONLY | O_CREAT | O_TRUNC,
	    0600);
	if (fdcore < 0) {
		logmsg(LOG_ERR, "open(%s): %m", corename);
		nerr++;
		goto closefd;
	}

	if (compress)
		core = zdopen(fdcore, "w");
	else
		core = fdopen(fdcore, "w");
	if (core == NULL) {
		logmsg(LOG_ERR, "%s: %m", corename);
		(void)close(fdcore);
		nerr++;
		goto closefd;
	}
	fdcore = -1;

	snprintf(infoname, sizeof(infoname), "info.%d", bounds);
	if (write_header_info(xostdout, &kdhl, savedirfd, infoname, device,
	    bounds, status) != 0) {
		nerr++;
		goto closeall;
	}

	if (isencrypted) {
		dumpkey = calloc(1, dumpkeysize);
		if (dumpkey == NULL) {
			logmsg(LOG_ERR, "Unable to allocate kernel dump key.");
			nerr++;
			goto closeall;
		}

		if (read(fddev, dumpkey, dumpkeysize) != (ssize_t)dumpkeysize) {
			logmsg(LOG_ERR, "Unable to read kernel dump key: %m.");
			nerr++;
			goto closeall;
		}

		snprintf(keyname, sizeof(keyname), "key.%d", bounds);
		ret = writekey(savedirfd, keyname, dumpkey, dumpkeysize);
		explicit_bzero(dumpkey, dumpkeysize);
		if (!ret) {
			nerr++;
			goto closeall;
		}
	}

	logmsg(LOG_NOTICE, "writing %s%score to %s/%s",
	    isencrypted ? "encrypted " : "", compress ? "compressed " : "",
	    savedir, corename);

	if (istextdump) {
		if (DoTextdumpFile(fddev, dumplength, lasthd, buf, device,
		    corename, core) < 0)
			goto closeall;
	} else {
		if (DoRegularFile(fddev, dumplength, sectorsize,
		    !(compress || iscompressed || isencrypted),
		    uncompress ? kdhl.compression : KERNELDUMP_COMP_NONE,
		    buf, device, corename, core) < 0) {
			goto closeall;
		}
	}
	if (verbose)
		printf("\n");

	if (fclose(core) < 0) {
		logmsg(LOG_ERR, "error on %s: %m", corename);
		nerr++;
		goto closefd;
	}

	symlinks_remove(savedirfd);
	if (symlinkat(infoname, savedirfd, "info.last") == -1) {
		logmsg(LOG_WARNING, "unable to create symlink %s/%s: %m",
		    savedir, "info.last");
	}
	if (isencrypted) {
		if (symlinkat(keyname, savedirfd, "key.last") == -1) {
			logmsg(LOG_WARNING,
			    "unable to create symlink %s/%s: %m", savedir,
			    "key.last");
		}
	}
	if ((iscompressed && !uncompress) || compress) {
		snprintf(linkname, sizeof(linkname), "%s.last.%s",
		    istextdump ? "textdump.tar" :
		    (isencrypted ? "vmcore_encrypted" : "vmcore"),
		    (kdhl.compression == KERNELDUMP_COMP_ZSTD) ? "zst" : "gz");
	} else {
		snprintf(linkname, sizeof(linkname), "%s.last",
		    istextdump ? "textdump.tar" :
		    (isencrypted ? "vmcore_encrypted" : "vmcore"));
	}
	if (symlinkat(corename, savedirfd, linkname) == -1) {
		logmsg(LOG_WARNING, "unable to create symlink %s/%s: %m",
		    savedir, linkname);
	}

	nsaved++;

	if (verbose)
		printf("dump saved\n");

nuke:
	if (!keep) {
		if (verbose)
			printf("clearing dump header\n");
		memcpy(kdhl.magic, KERNELDUMPMAGIC_CLEARED, sizeof(kdhl.magic));
		memcpy(temp, &kdhl, sizeof(kdhl));
		if (lseek(fddev, lasthd, SEEK_SET) != lasthd ||
		    write(fddev, temp, sectorsize) != (ssize_t)sectorsize)
			logmsg(LOG_ERR,
			    "error while clearing the dump header: %m");
	}
	xo_close_container_h(xostdout, "crashdump");
	if (xo_finish_h(xostdout) < 0)
		xo_err(EXIT_FAILURE, "stdout");
	free(dumpkey);
	free(temp);
	close(fddev);
	return;

closeall:
	fclose(core);

closefd:
	free(dumpkey);
	free(temp);
	close(fddev);
}

/* Prepend "/dev/" to any arguments that don't already have it */
static char **
devify(int argc, char **argv)
{
	char **devs;
	int i, l;

	devs = malloc(argc * sizeof(*argv));
	if (devs == NULL) {
		logmsg(LOG_ERR, "malloc(): %m");
		exit(EXIT_FAILURE);
	}
	for (i = 0; i < argc; i++) {
		if (strncmp(argv[i], _PATH_DEV, sizeof(_PATH_DEV) - 1) == 0)
			devs[i] = strdup(argv[i]);
		else {
			char *fullpath;

			fullpath = malloc(PATH_MAX);
			if (fullpath == NULL) {
				logmsg(LOG_ERR, "malloc(): %m");
				exit(EXIT_FAILURE);
			}
			l = snprintf(fullpath, PATH_MAX, "%s%s", _PATH_DEV,
			    argv[i]);
			if (l < 0) {
				logmsg(LOG_ERR, "snprintf(): %m");
				exit(EXIT_FAILURE);
			} else if (l >= PATH_MAX) {
				logmsg(LOG_ERR, "device name too long");
				exit(EXIT_FAILURE);
			}
			devs[i] = fullpath;
		}
	}
	return (devs);
}

static char **
enum_dumpdevs(int *argcp)
{
	struct fstab *fsp;
	char **argv;
	int argc, n;

	/*
	 * We cannot use getfsent(3) in capability mode, so we must
	 * scan /etc/fstab and build up a list of candidate devices
	 * before proceeding.
	 */
	argc = 0;
	n = 8;
	argv = malloc(n * sizeof(*argv));
	if (argv == NULL) {
		logmsg(LOG_ERR, "malloc(): %m");
		exit(EXIT_FAILURE);
	}
	for (;;) {
		fsp = getfsent();
		if (fsp == NULL)
			break;
		if (strcmp(fsp->fs_vfstype, "swap") != 0 &&
		    strcmp(fsp->fs_vfstype, "dump") != 0)
			continue;
		if (argc >= n) {
			n *= 2;
			argv = realloc(argv, n * sizeof(*argv));
			if (argv == NULL) {
				logmsg(LOG_ERR, "realloc(): %m");
				exit(EXIT_FAILURE);
			}
		}
		argv[argc] = strdup(fsp->fs_spec);
		if (argv[argc] == NULL) {
			logmsg(LOG_ERR, "strdup(): %m");
			exit(EXIT_FAILURE);
		}
		argc++;
	}
	*argcp = argc;
	return (argv);
}

static void
init_caps(int argc, char **argv)
{
	cap_rights_t rights;
	cap_channel_t *capcas;

	capcas = cap_init();
	if (capcas == NULL) {
		logmsg(LOG_ERR, "cap_init(): %m");
		exit(EXIT_FAILURE);
	}
	/*
	 * The fileargs capability does not currently provide a way to limit
	 * ioctls.
	 */
	(void)cap_rights_init(&rights, CAP_PREAD, CAP_WRITE, CAP_IOCTL);
	capfa = fileargs_init(argc, argv, checkfor || keep ? O_RDONLY : O_RDWR,
	    0, &rights, FA_OPEN);
	if (capfa == NULL) {
		logmsg(LOG_ERR, "fileargs_init(): %m");
		exit(EXIT_FAILURE);
	}
	caph_cache_catpages();
	caph_cache_tzdata();
	if (caph_enter_casper() != 0) {
		logmsg(LOG_ERR, "caph_enter_casper(): %m");
		exit(EXIT_FAILURE);
	}
	capsyslog = cap_service_open(capcas, "system.syslog");
	if (capsyslog == NULL) {
		logmsg(LOG_ERR, "cap_service_open(system.syslog): %m");
		exit(EXIT_FAILURE);
	}
	cap_close(capcas);
	cap_openlog(capsyslog, "savecore", LOG_OPTIONS, LOG_FACILITY);
}

static void
usage(void)
{
	xo_error("%s\n%s\n%s\n%s\n",
	    "usage: savecore -c [-v] [device ...]",
	    "       savecore -C [-v] [device ...]",
	    "       savecore -L [-fvZz] [-m maxdumps] [directory]",
	    "       savecore [-fkuvz] [-m maxdumps] [directory [device ...]]");
	exit(EXIT_FAILURE);
}

int
main(int argc, char **argv)
{
	cap_rights_t rights;
	const char *savedir;
	char **devs;
	int i, ch, error, savedirfd;

	checkfor = compress = clear = force = keep = livecore = false;
	verbose = 0;
	nfound = nsaved = nerr = 0;
	savedir = ".";
	comp_desired = KERNELDUMP_COMP_NONE;

	openlog("savecore", LOG_OPTIONS, LOG_FACILITY);
	signal(SIGINFO, infohandler);

	argc = xo_parse_args(argc, argv);
	if (argc < 0)
		exit(EXIT_FAILURE);

	while ((ch = getopt(argc, argv, "CcfkLm:uvZz")) != -1)
		switch(ch) {
		case 'C':
			checkfor = true;
			break;
		case 'c':
			clear = true;
			break;
		case 'f':
			force = true;
			break;
		case 'k':
			keep = true;
			break;
		case 'L':
			livecore = true;
			break;
		case 'm':
			maxdumps = atoi(optarg);
			if (maxdumps <= 0) {
				logmsg(LOG_ERR, "Invalid maxdump value");
				exit(EXIT_FAILURE);
			}
			break;
		case 'u':
			uncompress = true;
			break;
		case 'v':
			verbose++;
			break;
		case 'Z':
			/* No on-the-fly compression with zstd at the moment. */
			if (!livecore)
				usage();
			compress = true;
			comp_desired = KERNELDUMP_COMP_ZSTD;
			break;
		case 'z':
			compress = true;
			comp_desired = KERNELDUMP_COMP_GZIP;
			break;
		case '?':
		default:
			usage();
		}
	if (checkfor && (clear || force || keep))
		usage();
	if (clear && (compress || keep))
		usage();
	if (maxdumps > 0 && (checkfor || clear))
		usage();
	if (compress && uncompress)
		usage();
	if (livecore && (checkfor || clear || uncompress || keep))
		usage();
	argc -= optind;
	argv += optind;
	if (argc >= 1 && !checkfor && !clear) {
		error = chdir(argv[0]);
		if (error) {
			logmsg(LOG_ERR, "chdir(%s): %m", argv[0]);
			exit(EXIT_FAILURE);
		}
		savedir = argv[0];
		argc--;
		argv++;
	}
	if (livecore) {
		if (argc > 0)
			usage();

		/* Always need /dev/mem to invoke the dump */
		devs = malloc(sizeof(char *));
		devs[0] = strdup("/dev/mem");
		argc++;
	} else if (argc == 0)
		devs = enum_dumpdevs(&argc);
	else
		devs = devify(argc, argv);

	savedirfd = open(savedir, O_RDONLY | O_DIRECTORY);
	if (savedirfd < 0) {
		logmsg(LOG_ERR, "open(%s): %m", savedir);
		exit(EXIT_FAILURE);
	}
	(void)cap_rights_init(&rights, CAP_CREATE, CAP_FCNTL, CAP_FSTATAT,
	    CAP_FSTATFS, CAP_PREAD, CAP_SYMLINKAT, CAP_FTRUNCATE, CAP_UNLINKAT,
	    CAP_WRITE);
	if (livecore)
		cap_rights_set(&rights, CAP_RENAMEAT_SOURCE,
		    CAP_RENAMEAT_TARGET);
	if (caph_rights_limit(savedirfd, &rights) < 0) {
		logmsg(LOG_ERR, "cap_rights_limit(): %m");
		exit(EXIT_FAILURE);
	}

	/* Enter capability mode. */
	init_caps(argc, devs);

	for (i = 0; i < argc; i++)
		DoFile(savedir, savedirfd, devs[i]);

	if (nfound == 0) {
		if (checkfor) {
			if (verbose)
				printf("No dump exists\n");
			exit(EXIT_FAILURE);
		}
		if (verbose)
			logmsg(LOG_WARNING, "no dumps found");
	} else if (nsaved == 0) {
		if (nerr != 0) {
			if (verbose)
				logmsg(LOG_WARNING,
				    "unsaved dumps found but not saved");
			exit(EXIT_FAILURE);
		} else if (verbose)
			logmsg(LOG_WARNING, "no unsaved dumps found");
	} else if (verbose) {
		logmsg(LOG_NOTICE, "%d cores saved in %s\n", nsaved, savedir);
	}

	exit(EXIT_SUCCESS);
}

static void
infohandler(int sig __unused)
{
	got_siginfo = 1;
}
