/*-
 * Copyright (c) 2001 Doug Rabson
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

#include <sys/param.h>
#include <sys/time.h>
#include <stddef.h>
#include <stand.h>
#include <stdarg.h>

#include <bootstrap.h>
#include "libski.h"

struct disk_req {
	unsigned long addr;
	unsigned len;
};

struct disk_stat {
	int fd;
	unsigned count;
};

static int
skifs_open(const char *path, struct open_file *f)
{
	int fd;

	/*
	 * Skip leading '/' so that our pretend filesystem starts in
	 * the current working directory.
	 */
	while (*path == '/')
		path++;

	fd = ssc((u_int64_t) path, 1, 0, 0, SSC_OPEN);
	if (fd > 0) {
		f->f_fsdata = (void*)(u_int64_t) fd;
		return 0;
	}
	return ENOENT;
}

static int
skifs_close(struct open_file *f)
{
	ssc((u_int64_t) f->f_fsdata, 0, 0, 0, SSC_CLOSE);
	return 0;
}

static int
skifs_read(struct open_file *f, void *buf, size_t size, size_t *resid)
{
	struct disk_req req;
	struct disk_stat stat;

	req.len = size;
	req.addr = (u_int64_t) buf;
	ssc((u_int64_t) f->f_fsdata, 1, (u_int64_t) &req, f->f_offset, SSC_READ);
	stat.fd = (u_int64_t) f->f_fsdata;
	ssc((u_int64_t)&stat, 0, 0, 0, SSC_WAIT_COMPLETION);

	*resid = size - stat.count;
	f->f_offset += stat.count;
	return 0;
}

static off_t
skifs_seek(struct open_file *f, off_t offset, int where)
{
	u_int64_t base;

	switch (where) {
	case SEEK_SET:
		base = 0;
		break;

	case SEEK_CUR:
		base = f->f_offset;
		break;

	case SEEK_END:
		printf("can't find end of file in SKI\n");
		base = f->f_offset;
		break;
	}

	f->f_offset = base + offset;
	return base;
}

static int
skifs_stat(struct open_file *f, struct stat *sb)
{
	bzero(sb, sizeof(*sb));
	sb->st_mode = S_IFREG | S_IRUSR;
	return 0;
}

static int
skifs_readdir(struct open_file *f, struct dirent *d)
{
	return ENOENT;
}

struct fs_ops ski_fsops = {
	"fs",
	skifs_open,
	skifs_close,
	skifs_read,
	null_write,
	skifs_seek,
	skifs_stat,
	skifs_readdir
};

static int
skifs_dev_init(void) 
{
	return 0;
}

/*
 * Print information about disks
 */
static void
skifs_dev_print(int verbose)
{
}

/*
 * Attempt to open the disk described by (dev) for use by (f).
 *
 * Note that the philosophy here is "give them exactly what
 * they ask for".  This is necessary because being too "smart"
 * about what the user might want leads to complications.
 * (eg. given no slice or partition value, with a disk that is
 *  sliced - are they after the first BSD slice, or the DOS
 *  slice before it?)
 */
static int 
skifs_dev_open(struct open_file *f, ...)
{
	return 0;
}

static int 
skifs_dev_close(struct open_file *f)
{

	return 0;
}

static int 
skifs_dev_strategy(void *devdata, int rw, daddr_t dblk, size_t size, char *buf, size_t *rsize)
{
	return 0;
}

struct devsw skifs_dev = {
	"fs", 
	DEVT_DISK, 
	skifs_dev_init,
	skifs_dev_strategy, 
	skifs_dev_open, 
	skifs_dev_close, 
	noioctl,
	skifs_dev_print
};
