/*	$NetBSD: tftp.c,v 1.4 1997/09/17 16:57:07 drochner Exp $	 */

/*
 * Copyright (c) 1996
 *	Matthias Drochner.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed for the NetBSD Project
 *	by Matthias Drochner.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/*
 * Simple TFTP implementation for libsa.
 * Assumes:
 *  - socket descriptor (int) at open_file->f_devdata
 *  - server host IP in global servip
 * Restrictions:
 *  - read only
 *  - lseek only with SEEK_SET or SEEK_CUR
 *  - no big time differences between transfers (<tftp timeout)
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netinet/udp.h>
#include <netinet/in_systm.h>
#include <arpa/tftp.h>

#include <string.h>

#include "stand.h"
#include "net.h"
#include "netif.h"

#include "tftp.h"

static int	tftp_open(const char *path, struct open_file *f);
static int	tftp_close(struct open_file *f);
static int	tftp_read(struct open_file *f, void *buf, size_t size, size_t *resid);
static int	tftp_write(struct open_file *f, void *buf, size_t size, size_t *resid);
static off_t	tftp_seek(struct open_file *f, off_t offset, int where);
static int	tftp_stat(struct open_file *f, struct stat *sb);

struct fs_ops tftp_fsops = {
	"tftp",
	tftp_open,
	tftp_close,
	tftp_read,
	tftp_write,
	tftp_seek,
	tftp_stat,
	null_readdir
};

extern struct in_addr servip;

static int      tftpport = 2000;

#define RSPACE 520		/* max data packet, rounded up */

struct tftp_handle {
	struct iodesc  *iodesc;
	int             currblock;	/* contents of lastdata */
	int             islastblock;	/* flag */
	int             validsize;
	int             off;
	char           *path;	/* saved for re-requests */
	struct {
		u_char header[HEADER_SIZE];
		struct tftphdr t;
		u_char space[RSPACE];
	} lastdata;
};

static int tftperrors[8] = {
	0,			/* ??? */
	ENOENT,
	EPERM,
	ENOSPC,
	EINVAL,			/* ??? */
	EINVAL,			/* ??? */
	EEXIST,
	EINVAL			/* ??? */
};

static ssize_t 
recvtftp(d, pkt, len, tleft)
	register struct iodesc *d;
	register void  *pkt;
	register ssize_t len;
	time_t          tleft;
{
	struct tftphdr *t;

	errno = 0;

	len = readudp(d, pkt, len, tleft);

	if (len < 4)
		return (-1);

	t = (struct tftphdr *) pkt;
	switch (ntohs(t->th_opcode)) {
	case DATA: {
		int got;

		if (htons(t->th_block) != d->xid) {
			/*
			 * Expected block?
			 */
			return (-1);
		}
		if (d->xid == 1) {
			/*
			 * First data packet from new port.
			 */
			register struct udphdr *uh;
			uh = (struct udphdr *) pkt - 1;
			d->destport = uh->uh_sport;
		} /* else check uh_sport has not changed??? */
		got = len - (t->th_data - (char *) t);
		return got;
	}
	case ERROR:
		if ((unsigned) ntohs(t->th_code) >= 8) {
			printf("illegal tftp error %d\n", ntohs(t->th_code));
			errno = EIO;
		} else {
#ifdef DEBUG
			printf("tftp-error %d\n", ntohs(t->th_code));
#endif
			errno = tftperrors[ntohs(t->th_code)];
		}
		return (-1);
	default:
#ifdef DEBUG
		printf("tftp type %d not handled\n", ntohs(t->th_opcode));
#endif
		return (-1);
	}
}

/* send request, expect first block (or error) */
static int 
tftp_makereq(h)
	struct tftp_handle *h;
{
	struct {
		u_char header[HEADER_SIZE];
		struct tftphdr  t;
		u_char space[FNAME_SIZE + 6];
	} wbuf;
	char           *wtail;
	int             l;
	ssize_t         res;
	struct tftphdr *t;

	wbuf.t.th_opcode = htons((u_short) RRQ);
	wtail = wbuf.t.th_stuff;
	l = strlen(h->path);
	bcopy(h->path, wtail, l + 1);
	wtail += l + 1;
	bcopy("octet", wtail, 6);
	wtail += 6;

	t = &h->lastdata.t;

	/* h->iodesc->myport = htons(--tftpport); */
	h->iodesc->myport = htons(tftpport + (getsecs() & 0x3ff));
	h->iodesc->destport = htons(IPPORT_TFTP);
	h->iodesc->xid = 1;	/* expected block */

	res = sendrecv(h->iodesc, sendudp, &wbuf.t, wtail - (char *) &wbuf.t,
		       recvtftp, t, sizeof(*t) + RSPACE);

	if (res == -1)
		return (errno);

	h->currblock = 1;
	h->validsize = res;
	h->islastblock = 0;
	if (res < SEGSIZE)
		h->islastblock = 1;	/* very short file */
	return (0);
}

/* ack block, expect next */
static int 
tftp_getnextblock(h)
	struct tftp_handle *h;
{
	struct {
		u_char header[HEADER_SIZE];
		struct tftphdr t;
	} wbuf;
	char           *wtail;
	int             res;
	struct tftphdr *t;

	wbuf.t.th_opcode = htons((u_short) ACK);
	wtail = (char *) &wbuf.t.th_block;
	wbuf.t.th_block = htons((u_short) h->currblock);
	wtail += 2;

	t = &h->lastdata.t;

	h->iodesc->xid = h->currblock + 1;	/* expected block */

	res = sendrecv(h->iodesc, sendudp, &wbuf.t, wtail - (char *) &wbuf.t,
		       recvtftp, t, sizeof(*t) + RSPACE);

	if (res == -1)		/* 0 is OK! */
		return (errno);

	h->currblock++;
	h->validsize = res;
	if (res < SEGSIZE)
		h->islastblock = 1;	/* EOF */
	return (0);
}

static int 
tftp_open(path, f)
	const char *path;
	struct open_file *f;
{
	struct tftp_handle *tftpfile;
	struct iodesc  *io;
	int             res;

	tftpfile = (struct tftp_handle *) malloc(sizeof(*tftpfile));
	if (!tftpfile)
		return (ENOMEM);

	tftpfile->iodesc = io = socktodesc(*(int *) (f->f_devdata));
	if (io == NULL)
		return (EINVAL);

	io->destip = servip;
	tftpfile->off = 0;
	tftpfile->path = strdup(path);
	if (tftpfile->path == NULL) {
	    free(tftpfile);
	    return(ENOMEM);
	}

	res = tftp_makereq(tftpfile, path);

	if (res) {
		free(tftpfile->path);
		free(tftpfile);
		return (res);
	}
	f->f_fsdata = (void *) tftpfile;
	return (0);
}

static int 
tftp_read(f, addr, size, resid)
	struct open_file *f;
	void           *addr;
	size_t          size;
	size_t         *resid;	/* out */
{
	struct tftp_handle *tftpfile;
	static int      tc = 0;
	tftpfile = (struct tftp_handle *) f->f_fsdata;

	while (size > 0) {
		int needblock, count;

		if (!(tc++ % 16))
			twiddle();

		needblock = tftpfile->off / SEGSIZE + 1;

		if (tftpfile->currblock > needblock)	/* seek backwards */
			tftp_makereq(tftpfile);	/* no error check, it worked
						 * for open */

		while (tftpfile->currblock < needblock) {
			int res;

			res = tftp_getnextblock(tftpfile);
			if (res) {	/* no answer */
#ifdef DEBUG
				printf("tftp: read error\n");
#endif
				return (res);
			}
			if (tftpfile->islastblock)
				break;
		}

		if (tftpfile->currblock == needblock) {
			int offinblock, inbuffer;

			offinblock = tftpfile->off % SEGSIZE;

			inbuffer = tftpfile->validsize - offinblock;
			if (inbuffer < 0) {
#ifdef DEBUG
				printf("tftp: invalid offset %d\n",
				    tftpfile->off);
#endif
				return (EINVAL);
			}
			count = (size < inbuffer ? size : inbuffer);
			bcopy(tftpfile->lastdata.t.th_data + offinblock,
			    addr, count);

			addr += count;
			tftpfile->off += count;
			size -= count;

			if ((tftpfile->islastblock) && (count == inbuffer))
				break;	/* EOF */
		} else {
#ifdef DEBUG
			printf("tftp: block %d not found\n", needblock);
#endif
			return (EINVAL);
		}

	}

	if (resid)
		*resid = size;
	return (0);
}

static int 
tftp_close(f)
	struct open_file *f;
{
	struct tftp_handle *tftpfile;
	tftpfile = (struct tftp_handle *) f->f_fsdata;

	/* let it time out ... */

	if (tftpfile) {
		free(tftpfile->path);
		free(tftpfile);
	}
	return (0);
}

static int 
tftp_write(f, start, size, resid)
	struct open_file *f;
	void           *start;
	size_t          size;
	size_t         *resid;	/* out */
{
	return (EROFS);
}

static int 
tftp_stat(f, sb)
	struct open_file *f;
	struct stat    *sb;
{
	struct tftp_handle *tftpfile;
	tftpfile = (struct tftp_handle *) f->f_fsdata;

	sb->st_mode = 0444 | S_IFREG;
	sb->st_nlink = 1;
	sb->st_uid = 0;
	sb->st_gid = 0;
	sb->st_size = -1;
	return (0);
}

static off_t 
tftp_seek(f, offset, where)
	struct open_file *f;
	off_t           offset;
	int             where;
{
	struct tftp_handle *tftpfile;
	tftpfile = (struct tftp_handle *) f->f_fsdata;

	switch (where) {
	case SEEK_SET:
		tftpfile->off = offset;
		break;
	case SEEK_CUR:
		tftpfile->off += offset;
		break;
	default:
		errno = EOFFSET;
		return (-1);
	}
	return (tftpfile->off);
}
