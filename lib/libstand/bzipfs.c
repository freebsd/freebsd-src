/* 
 * Copyright (c) 1998 Michael Smith.
 * Copyright (c) 2000 Maxim Sobolev
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "stand.h"

#include <sys/stat.h>
#include <string.h>
#include <_bzlib.h>

#define BZ_BUFSIZE 2048	/* XXX larger? */

struct bz_file
{
    int			bzf_rawfd;
    bz_stream		bzf_bzstream;
    char		bzf_buf[BZ_BUFSIZE];
};

static int	bzf_fill(struct bz_file *z);
static int	bzf_open(const char *path, struct open_file *f);
static int	bzf_close(struct open_file *f);
static int	bzf_read(struct open_file *f, void *buf, size_t size, size_t *resid);
static off_t	bzf_seek(struct open_file *f, off_t offset, int where);
static int	bzf_stat(struct open_file *f, struct stat *sb);

struct fs_ops bzipfs_fsops = {
    "bzip",
    bzf_open, 
    bzf_close, 
    bzf_read,
    null_write,
    bzf_seek,
    bzf_stat,
    null_readdir
};

#if 0
void *
calloc(int items, size_t size)
{
    return(malloc(items * size));
}
#endif

static int
bzf_fill(struct bz_file *bzf)
{
    int		result;
    int		req;
    
    req = BZ_BUFSIZE - bzf->bzf_bzstream.avail_in;
    result = 0;
    
    /* If we need more */
    if (req > 0) {
	/* move old data to bottom of buffer */
	if (req < BZ_BUFSIZE)
	    bcopy(bzf->bzf_buf + req, bzf->bzf_buf, BZ_BUFSIZE - req);
	
	/* read to fill buffer and update availibility data */
	result = read(bzf->bzf_rawfd, bzf->bzf_buf + bzf->bzf_bzstream.avail_in, req);
	bzf->bzf_bzstream.next_in = bzf->bzf_buf;
	if (result >= 0)
	    bzf->bzf_bzstream.avail_in += result;
    }
    return(result);
}

/*
 * Adapted from get_byte/check_header in libz
 *
 * Returns 0 if the header is OK, nonzero if not.
 */
static int
get_byte(struct bz_file *bzf)
{
    if ((bzf->bzf_bzstream.avail_in == 0) && (bzf_fill(bzf) == -1))
	return(-1);
    bzf->bzf_bzstream.avail_in--;
    return(*(bzf->bzf_bzstream.next_in)++);
}

static int bz_magic[3] = {'B', 'Z', 'h'}; /* bzip2 magic header */

static int
check_header(struct bz_file *bzf)
{
    unsigned int len;
    int		 c;

    /* Check the bzip2 magic header */
    for (len = 0; len < 3; len++) {
	c = get_byte(bzf);
	if (c != bz_magic[len]) {
	    return(1);
	}
    }
    /* Check that the block size is valid */
    c = get_byte(bzf);
    if (c < '1' || c > '9')
	return(1);

    /* Put back bytes that we've took from the input stream */
    bzf->bzf_bzstream.next_in -= 4;
    bzf->bzf_bzstream.avail_in += 4;

    return(0);
}
	
static int
bzf_open(const char *fname, struct open_file *f)
{
    static char		*bzfname;
    int			rawfd;
    struct bz_file	*bzf;
    char		*cp;
    int			error;
    struct stat		sb;

    /* Have to be in "just read it" mode */
    if (f->f_flags != F_READ)
	return(EPERM);

    /* If the name already ends in .gz or .bz2, ignore it */
    if ((cp = strrchr(fname, '.')) && (!strcmp(cp, ".gz")
	    || !strcmp(cp, ".bz2") || !strcmp(cp, ".split")))
	return(ENOENT);

    /* Construct new name */
    bzfname = malloc(strlen(fname) + 5);
    sprintf(bzfname, "%s.bz2", fname);

    /* Try to open the compressed datafile */
    rawfd = open(bzfname, O_RDONLY);
    free(bzfname);
    if (rawfd == -1)
	return(ENOENT);

    if (fstat(rawfd, &sb) < 0) {
	printf("bzf_open: stat failed\n");
	close(rawfd);
	return(ENOENT);
    }
    if (!S_ISREG(sb.st_mode)) {
	printf("bzf_open: not a file\n");
	close(rawfd);
	return(EISDIR);			/* best guess */
    }

    /* Allocate a bz_file structure, populate it */
    bzf = malloc(sizeof(struct bz_file));
    bzero(bzf, sizeof(struct bz_file));
    bzf->bzf_rawfd = rawfd;

    /* Verify that the file is bzipped (XXX why do this afterwards?) */
    if (check_header(bzf)) {
	close(bzf->bzf_rawfd);
	BZ2_bzDecompressEnd(&(bzf->bzf_bzstream));
	free(bzf);
	return(EFTYPE);
    }

    /* Initialise the inflation engine */
    if ((error = BZ2_bzDecompressInit(&(bzf->bzf_bzstream), 0, 1)) != BZ_OK) {
	printf("bzf_open: BZ2_bzDecompressInit returned %d\n", error);
	close(bzf->bzf_rawfd);
	free(bzf);
	return(EIO);
    }

    /* Looks OK, we'll take it */
    f->f_fsdata = bzf;
    return(0);
}

static int
bzf_close(struct open_file *f)
{
    struct bz_file	*bzf = (struct bz_file *)f->f_fsdata;
    
    BZ2_bzDecompressEnd(&(bzf->bzf_bzstream));
    close(bzf->bzf_rawfd);
    free(bzf);
    return(0);
}
 
static int 
bzf_read(struct open_file *f, void *buf, size_t size, size_t *resid)
{
    struct bz_file	*bzf = (struct bz_file *)f->f_fsdata;
    int			error;

    bzf->bzf_bzstream.next_out = buf;			/* where and how much */
    bzf->bzf_bzstream.avail_out = size;

    while (bzf->bzf_bzstream.avail_out) {
	if ((bzf->bzf_bzstream.avail_in == 0) && (bzf_fill(bzf) == -1)) {
	    printf("bzf_read: fill error\n");
	    return(-1);
	}
	if (bzf->bzf_bzstream.avail_in == 0) {		/* oops, unexpected EOF */
	    printf("bzf_read: unexpected EOF\n");
	    break;
	}

	error = BZ2_bzDecompress(&bzf->bzf_bzstream);	/* decompression pass */
	if (error == BZ_STREAM_END) {			/* EOF, all done */
	    break;
	}
	if (error != BZ_OK) {				/* argh, decompression error */
	    printf("bzf_read: BZ2_bzDecompress returned %d\n", error);
	    errno = EIO;
	    return(-1);
	}
    }
    if (resid != NULL)
	*resid = bzf->bzf_bzstream.avail_out;
    return(0);
}

static off_t
bzf_seek(struct open_file *f, off_t offset, int where)
{
    struct bz_file	*bzf = (struct bz_file *)f->f_fsdata;
    off_t		target;
    char		discard[16];
    
    switch (where) {
    case SEEK_SET:
	target = offset;
	break;
    case SEEK_CUR:
	target = offset + bzf->bzf_bzstream.total_out_lo32;
	break;
    default:
	target = -1;
    }

    /* Can we get there from here? */
    if (target < bzf->bzf_bzstream.total_out_lo32) {
	errno = EOFFSET;
	return -1;
    } 

    /* skip forwards if required */
    while (target > bzf->bzf_bzstream.total_out_lo32) {
	if (bzf_read(f, discard, min(sizeof(discard), target - bzf->bzf_bzstream.total_out_lo32), NULL) == -1)
	    return(-1);
    }
    /* This is where we are (be honest if we overshot) */
    return (bzf->bzf_bzstream.total_out_lo32);
}

static int
bzf_stat(struct open_file *f, struct stat *sb)
{
    struct bz_file	*bzf = (struct bz_file *)f->f_fsdata;
    int			result;

    /* stat as normal, but indicate that size is unknown */
    if ((result = fstat(bzf->bzf_rawfd, sb)) == 0)
	sb->st_size = -1;
    return(result);
}

void
bz_internal_error(int errorcode)
{
    panic("bzipfs: critical error %d in bzip2 library occured\n", errorcode);
}
