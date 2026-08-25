/*
 * Copyright (c) 1998 Michael Smith.
 * Copyright (c) 2026 Netflix, Inc.
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

/*
 * Stacked filesystem for .zst compressed files, structured like gzipfs.c and
 * bzipfs.c above it. Only built when the zstd decompressor is already being
 * pulled into libsa for ZFS (see libsa/Makefile), since that's the only
 * source of the ZSTD_* symbols in this environment.
 */

#include "stand.h"

#include <sys/stat.h>
#include <string.h>
#include <zstd.h>

#define ZSTD_BUFSIZE 2048	/* XXX larger? */

struct zstd_file
{
    int			zstdf_rawfd;
    ZSTD_DStream	*zstdf_strm;
    ZSTD_inBuffer	zstdf_in;
    unsigned char	zstdf_inbuf[ZSTD_BUFSIZE];
    int			zstdf_endseen;
    off_t		zstdf_total_out;
};

static int	zstdf_fill(struct zstd_file *zstdf);
static int	zstdf_open(const char *path, struct open_file *f);
static int	zstdf_close(struct open_file *f);
static int	zstdf_read(struct open_file *f, void *buf, size_t size, size_t *resid);
static off_t	zstdf_seek(struct open_file *f, off_t offset, int where);
static int	zstdf_stat(struct open_file *f, struct stat *sb);

struct fs_ops zstdfs_fsops = {
	.fs_name = "zstd",
	.fs_flags = 0,
	.fo_open = zstdf_open,
	.fo_close = zstdf_close,
	.fo_read = zstdf_read,
	.fo_write = null_write,
	.fo_seek = zstdf_seek,
	.fo_stat = zstdf_stat,
	.fo_readdir = null_readdir,
};

static int
zstdf_fill(struct zstd_file *zstdf)
{
    int		result;
    int		avail_in;
    int		req;

    avail_in = zstdf->zstdf_in.size - zstdf->zstdf_in.pos;
    req = ZSTD_BUFSIZE - avail_in;
    result = 0;

    /* If we need more */
    if (req > 0) {
	/* move old data to bottom of buffer */
	if (avail_in > 0)
	    bcopy(zstdf->zstdf_inbuf + zstdf->zstdf_in.pos, zstdf->zstdf_inbuf, avail_in);

	/* read to fill buffer and update availibility data */
	result = read(zstdf->zstdf_rawfd, zstdf->zstdf_inbuf + avail_in, req);
	zstdf->zstdf_in.src = zstdf->zstdf_inbuf;
	zstdf->zstdf_in.pos = 0;
	zstdf->zstdf_in.size = avail_in + (result >= 0 ? result : 0);
    }
    return (result);
}

static const unsigned char zstd_magic[4] = {0x28, 0xb5, 0x2f, 0xfd};

/*
 * Peek at the fixed-size zstd frame magic without consuming it, so the
 * header remains for ZSTD_decompressStream() to parse normally on the
 * first read.
 *
 * Returns 0 if the header is OK, nonzero if not.
 */
static int
check_header(struct zstd_file *zstdf)
{
    if (zstdf->zstdf_in.size - zstdf->zstdf_in.pos < sizeof(zstd_magic) &&
	zstdf_fill(zstdf) == -1)
	return (1);
    if (zstdf->zstdf_in.size - zstdf->zstdf_in.pos < sizeof(zstd_magic))
	return (1);
    return (memcmp((const unsigned char *)zstdf->zstdf_in.src + zstdf->zstdf_in.pos,
	zstd_magic, sizeof(zstd_magic)) != 0);
}

static int
zstdf_open(const char *fname, struct open_file *f)
{
    static char		*zstdfname;
    int			rawfd;
    struct zstd_file	*zstdf;
    char		*cp;
    struct stat		sb;

    /* Have to be in "just read it" mode */
    if (f->f_flags != F_READ)
	return(EPERM);

    /* If the name already ends in a known compressed suffix, ignore it */
    if ((cp = strrchr(fname, '.')) && (!strcmp(cp, ".gz")
	    || !strcmp(cp, ".bz2") || !strcmp(cp, ".xz")
	    || !strcmp(cp, ".zst") || !strcmp(cp, ".split")))
	return(ENOENT);

    /* Construct new name */
    zstdfname = malloc(strlen(fname) + 5);
    if (zstdfname == NULL)
        return(ENOMEM);
    sprintf(zstdfname, "%s.zst", fname);

    /* Try to open the compressed datafile */
    rawfd = open(zstdfname, O_RDONLY);
    free(zstdfname);
    if (rawfd == -1)
	return(ENOENT);

    if (fstat(rawfd, &sb) < 0) {
	printf("zstdf_open: stat failed\n");
	close(rawfd);
	return(ENOENT);
    }
    if (!S_ISREG(sb.st_mode)) {
	printf("zstdf_open: not a file\n");
	close(rawfd);
	return(EISDIR);			/* best guess */
    }

    /* Allocate a zstd_file structure, populate it */
    zstdf = malloc(sizeof(struct zstd_file));
    if (zstdf == NULL) {
	close(rawfd);
        return(ENOMEM);
    }
    bzero(zstdf, sizeof(struct zstd_file));
    zstdf->zstdf_rawfd = rawfd;
    zstdf->zstdf_in.src = zstdf->zstdf_inbuf;

    /* Verify that the file is zstd compressed */
    if (check_header(zstdf)) {
	close(zstdf->zstdf_rawfd);
	free(zstdf);
	return(EFTYPE);
    }

    /* Initialise the inflation engine */
    zstdf->zstdf_strm = ZSTD_createDStream();
    if (zstdf->zstdf_strm == NULL) {
	close(zstdf->zstdf_rawfd);
	free(zstdf);
	return(ENOMEM);
    }
    if (ZSTD_isError(ZSTD_initDStream(zstdf->zstdf_strm))) {
	ZSTD_freeDStream(zstdf->zstdf_strm);
	close(zstdf->zstdf_rawfd);
	free(zstdf);
	return(EIO);
    }

    /* Looks OK, we'll take it */
    f->f_fsdata = zstdf;
    return(0);
}

static int
zstdf_close(struct open_file *f)
{
    struct zstd_file	*zstdf = (struct zstd_file *)f->f_fsdata;

    ZSTD_freeDStream(zstdf->zstdf_strm);
    close(zstdf->zstdf_rawfd);
    free(zstdf);
    return(0);
}

static int
zstdf_read(struct open_file *f, void *buf, size_t size, size_t *resid)
{
    struct zstd_file	*zstdf = (struct zstd_file *)f->f_fsdata;
    ZSTD_outBuffer	out = { buf, size, 0 };
    size_t		ret;

    while (out.pos < out.size && zstdf->zstdf_endseen == 0) {
	if ((zstdf->zstdf_in.pos == zstdf->zstdf_in.size) && (zstdf_fill(zstdf) == -1)) {
	    printf("zstdf_read: fill error\n");
	    return(EIO);
	}
	if (zstdf->zstdf_in.pos == zstdf->zstdf_in.size) {	/* oops, unexpected EOF */
	    printf("zstdf_read: unexpected EOF\n");
	    if (out.pos == 0)
		return(EIO);
	    break;
	}

	ret = ZSTD_decompressStream(zstdf->zstdf_strm, &out, &zstdf->zstdf_in);
	if (ZSTD_isError(ret)) {			/* argh, decompression error */
	    printf("zstdf_read: %s\n", ZSTD_getErrorName(ret));
	    return(EIO);
	}
	if (ret == 0) {				/* EOF, all done */
	    zstdf->zstdf_endseen = 1;
	    break;
	}
    }
    zstdf->zstdf_total_out += out.pos;
    if (resid != NULL)
	*resid = out.size - out.pos;
    return(0);
}

static int
zstdf_rewind(struct open_file *f)
{
    struct zstd_file	*zstdf = (struct zstd_file *)f->f_fsdata;

    if (lseek(zstdf->zstdf_rawfd, 0, SEEK_SET) == -1)
	return(-1);
    zstdf->zstdf_in.src = zstdf->zstdf_inbuf;
    zstdf->zstdf_in.pos = 0;
    zstdf->zstdf_in.size = 0;
    zstdf->zstdf_endseen = 0;
    zstdf->zstdf_total_out = 0;
    if (ZSTD_isError(ZSTD_initDStream(zstdf->zstdf_strm)))
	return(-1);

    return(0);
}

static off_t
zstdf_seek(struct open_file *f, off_t offset, int where)
{
    struct zstd_file	*zstdf = (struct zstd_file *)f->f_fsdata;
    off_t		target;
    char		discard[16];

    switch (where) {
    case SEEK_SET:
	target = offset;
	break;
    case SEEK_CUR:
	target = offset + zstdf->zstdf_total_out;
	break;
    default:
	errno = EINVAL;
	return(-1);
    }

    /* rewind if required */
    if (target < zstdf->zstdf_total_out && zstdf_rewind(f) != 0)
	return(-1);

    /* skip forwards if required */
    while (target > zstdf->zstdf_total_out) {
	errno = zstdf_read(f, discard, min(sizeof(discard),
	    target - zstdf->zstdf_total_out), NULL);
	if (errno)
	    return(-1);
	/* Break out of loop if end of file has been reached. */
	if (zstdf->zstdf_endseen)
	    break;
    }
    /* This is where we are (be honest if we overshot) */
    return(zstdf->zstdf_total_out);
}

static int
zstdf_stat(struct open_file *f, struct stat *sb)
{
    struct zstd_file	*zstdf = (struct zstd_file *)f->f_fsdata;
    int			result;

    /* stat as normal, but indicate that size is unknown */
    if ((result = fstat(zstdf->zstdf_rawfd, sb)) == 0)
	sb->st_size = -1;
    return(result);
}
