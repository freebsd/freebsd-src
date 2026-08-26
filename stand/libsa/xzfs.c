/*
 * Copyright (c) 1998 Michael Smith.
 * All rights reserved.
 * Copyright (c) 2026 Netflix, Inc.
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
 * Stacked filesystem for .xz compressed files, structured like gzipfs.c and
 * bzipfs.c above it.
 */

#include "stand.h"

#include <sys/stat.h>
#include <string.h>
#include <xz.h>

#define XZ_BUFSIZE 2048	/* XXX larger? */

struct xz_file
{
    int			xzf_rawfd;
    struct xz_dec	*xzf_strm;
    struct xz_buf	xzf_buf;
    unsigned char	xzf_inbuf[XZ_BUFSIZE];
    int			xzf_endseen;
    off_t		xzf_total_out;
};

static int	xzf_fill(struct xz_file *xzf);
static int	xzf_open(const char *path, struct open_file *f);
static int	xzf_close(struct open_file *f);
static int	xzf_read(struct open_file *f, void *buf, size_t size, size_t *resid);
static off_t	xzf_seek(struct open_file *f, off_t offset, int where);
static int	xzf_stat(struct open_file *f, struct stat *sb);

struct fs_ops xzfs_fsops = {
	.fs_name = "xz",
	.fs_flags = 0,
	.fo_open = xzf_open,
	.fo_close = xzf_close,
	.fo_read = xzf_read,
	.fo_write = null_write,
	.fo_seek = xzf_seek,
	.fo_stat = xzf_stat,
	.fo_readdir = null_readdir,
};

static int
xzf_fill(struct xz_file *xzf)
{
    int		result;
    int		avail_in;
    int		req;

    avail_in = xzf->xzf_buf.in_size - xzf->xzf_buf.in_pos;
    req = XZ_BUFSIZE - avail_in;
    result = 0;

    /* If we need more */
    if (req > 0) {
	/* move old data to bottom of buffer */
	if (avail_in > 0)
	    bcopy(xzf->xzf_inbuf + xzf->xzf_buf.in_pos, xzf->xzf_inbuf, avail_in);

	/* read to fill buffer and update availibility data */
	result = read(xzf->xzf_rawfd, xzf->xzf_inbuf + avail_in, req);
	xzf->xzf_buf.in = xzf->xzf_inbuf;
	xzf->xzf_buf.in_pos = 0;
	xzf->xzf_buf.in_size = avail_in + (result >= 0 ? result : 0);
    }
    return (result);
}

static const unsigned char xz_magic[6] = {0xfd, '7', 'z', 'X', 'Z', 0x00};

/*
 * Peek at the fixed-size .xz magic without consuming it, so the header
 * remains for xz_dec_run() to parse normally on the first read.
 *
 * Returns 0 if the header is OK, nonzero if not.
 */
static int
check_header(struct xz_file *xzf)
{
    if (xzf->xzf_buf.in_size - xzf->xzf_buf.in_pos < (int)sizeof(xz_magic) &&
	xzf_fill(xzf) == -1)
	return (1);
    if (xzf->xzf_buf.in_size - xzf->xzf_buf.in_pos < (int)sizeof(xz_magic))
	return (1);
    return (memcmp(xzf->xzf_buf.in + xzf->xzf_buf.in_pos, xz_magic,
	sizeof(xz_magic)) != 0);
}

static int
xzf_open(const char *fname, struct open_file *f)
{
    static char		*xzfname;
    int			rawfd;
    struct xz_file	*xzf;
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
    xzfname = malloc(strlen(fname) + 4);
    if (xzfname == NULL)
        return(ENOMEM);
    sprintf(xzfname, "%s.xz", fname);

    /* Try to open the compressed datafile */
    rawfd = open(xzfname, O_RDONLY);
    free(xzfname);
    if (rawfd == -1)
	return(ENOENT);

    if (fstat(rawfd, &sb) < 0) {
	printf("xzf_open: stat failed\n");
	close(rawfd);
	return(ENOENT);
    }
    if (!S_ISREG(sb.st_mode)) {
	printf("xzf_open: not a file\n");
	close(rawfd);
	return(EISDIR);			/* best guess */
    }

    /* Allocate an xz_file structure, populate it */
    xzf = malloc(sizeof(struct xz_file));
    if (xzf == NULL) {
	close(rawfd);
        return(ENOMEM);
    }
    bzero(xzf, sizeof(struct xz_file));
    xzf->xzf_rawfd = rawfd;
    xzf->xzf_buf.in = xzf->xzf_inbuf;

    /*
     * The embedded xz decoder's internal CRC32/CRC64 tables start out
     * zeroed and must be built before any stream can be decoded, or the
     * Stream Header CRC32 check fails on the very first read.
     */
    xz_crc32_init();
    xz_crc64_init();

    /* Verify that the file is xz compressed */
    if (check_header(xzf)) {
	close(xzf->xzf_rawfd);
	free(xzf);
	return(EFTYPE);
    }

    /* Initialise the inflation engine */
    xzf->xzf_strm = xz_dec_init(XZ_DYNALLOC, (uint32_t)-1);
    if (xzf->xzf_strm == NULL) {
	close(xzf->xzf_rawfd);
	free(xzf);
	return(EIO);
    }

    /* Looks OK, we'll take it */
    f->f_fsdata = xzf;
    return(0);
}

static int
xzf_close(struct open_file *f)
{
    struct xz_file	*xzf = (struct xz_file *)f->f_fsdata;

    xz_dec_end(xzf->xzf_strm);
    close(xzf->xzf_rawfd);
    free(xzf);
    return(0);
}

static int
xzf_read(struct open_file *f, void *buf, size_t size, size_t *resid)
{
    struct xz_file	*xzf = (struct xz_file *)f->f_fsdata;
    enum xz_ret		ret;

    xzf->xzf_buf.out = buf;			/* where and how much */
    xzf->xzf_buf.out_pos = 0;
    xzf->xzf_buf.out_size = size;

    while (xzf->xzf_buf.out_pos < xzf->xzf_buf.out_size && xzf->xzf_endseen == 0) {
	if ((xzf->xzf_buf.in_pos == xzf->xzf_buf.in_size) && (xzf_fill(xzf) == -1)) {
	    printf("xzf_read: fill error\n");
	    return(EIO);
	}
	if (xzf->xzf_buf.in_pos == xzf->xzf_buf.in_size) {	/* oops, unexpected EOF */
	    printf("xzf_read: unexpected EOF\n");
	    if (xzf->xzf_buf.out_pos == 0)
		return(EIO);
	    break;
	}

	ret = xz_dec_run(xzf->xzf_strm, &xzf->xzf_buf);	/* decompression pass */
	if (ret == XZ_STREAM_END) {			/* EOF, all done */
	    xzf->xzf_endseen = 1;
	    break;
	}
	if (ret != XZ_OK) {				/* argh, decompression error */
	    printf("xzf_read: xz_dec_run returned %d\n", ret);
	    return(EIO);
	}
    }
    xzf->xzf_total_out += xzf->xzf_buf.out_pos;
    if (resid != NULL)
	*resid = xzf->xzf_buf.out_size - xzf->xzf_buf.out_pos;
    return(0);
}

static int
xzf_rewind(struct open_file *f)
{
    struct xz_file	*xzf = (struct xz_file *)f->f_fsdata;

    if (lseek(xzf->xzf_rawfd, 0, SEEK_SET) == -1)
	return(-1);
    xzf->xzf_buf.in = xzf->xzf_inbuf;
    xzf->xzf_buf.in_pos = 0;
    xzf->xzf_buf.in_size = 0;
    xzf->xzf_endseen = 0;
    xzf->xzf_total_out = 0;
    xz_dec_reset(xzf->xzf_strm);

    return(0);
}

static off_t
xzf_seek(struct open_file *f, off_t offset, int where)
{
    struct xz_file	*xzf = (struct xz_file *)f->f_fsdata;
    off_t		target;
    char		discard[16];

    switch (where) {
    case SEEK_SET:
	target = offset;
	break;
    case SEEK_CUR:
	target = offset + xzf->xzf_total_out;
	break;
    default:
	errno = EINVAL;
	return(-1);
    }

    /* rewind if required */
    if (target < xzf->xzf_total_out && xzf_rewind(f) != 0)
	return(-1);

    /* skip forwards if required */
    while (target > xzf->xzf_total_out) {
	errno = xzf_read(f, discard, min(sizeof(discard),
	    target - xzf->xzf_total_out), NULL);
	if (errno)
	    return(-1);
	/* Break out of loop if end of file has been reached. */
	if (xzf->xzf_endseen)
	    break;
    }
    /* This is where we are (be honest if we overshot) */
    return(xzf->xzf_total_out);
}

static int
xzf_stat(struct open_file *f, struct stat *sb)
{
    struct xz_file	*xzf = (struct xz_file *)f->f_fsdata;
    int			result;

    /* stat as normal, but indicate that size is unknown */
    if ((result = fstat(xzf->xzf_rawfd, sb)) == 0)
	sb->st_size = -1;
    return(result);
}
