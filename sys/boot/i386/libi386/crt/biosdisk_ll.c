/*
 * $Id$
 * From: $NetBSD: biosdisk_ll.c,v 1.3 1997/06/13 13:36:06 drochner Exp $
 */

/*
 * Copyright (c) 1996
 * 	Matthias Drochner.  All rights reserved.
 * Copyright (c) 1996
 * 	Perry E. Metzger.  All rights reserved.
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
 *    must display the following acknowledgements:
 *	This product includes software developed for the NetBSD Project
 *	by Matthias Drochner.
 *	This product includes software developed for the NetBSD Project
 *	by Perry E. Metzger.
 * 4. The names of the authors may not be used to endorse or promote products
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
 */

/*
 * shared by bootsector startup (bootsectmain) and biosdisk.c
 * needs lowlevel parts from bios_disk.S
 */

#include <stand.h>

#include "biosdisk_ll.h"
#include "diskbuf.h"

/* XXX prototypes from export header? */
extern int get_diskinfo(int);
extern int biosread(int, int, int, int, int, char *);

#define	SPT(di)		((di)&0xff)
#define	HEADS(di)	((((di)>>8)&0xff)+1)

int 
set_geometry(d)
	struct biosdisk_ll *d;
{
	int             diskinfo;

	diskinfo = get_diskinfo(d->dev);

	d->spc = (d->spt = SPT(diskinfo)) * HEADS(diskinfo);

	/*
	 * get_diskinfo assumes floppy if BIOS call fails. Check at least
	 * "valid" geometry.
	 */
	return (!d->spc || !d->spt);
}

/*
 * Global shared "diskbuf" is used as read ahead buffer.  For reading from
 * floppies, the bootstrap has to be loaded on a 64K boundary to ensure that
 * this buffer doesn't cross a 64K DMA boundary.
 */
static int      ra_dev;
static int      ra_end;
static int      ra_first;

int 
readsects(d, dblk, num, buf, cold)	/* reads ahead if (!cold) */
	struct biosdisk_ll *d;
	int             dblk, num;
	char           *buf;
	int             cold;	/* don't use data segment or bss, don't call
				 * library functions */
{
	while (num) {
		int             nsec;

		/* check for usable data in read-ahead buffer */
		if (cold || diskbuf_user != &ra_dev || d->dev != ra_dev
		    || dblk < ra_first || dblk >= ra_end) {

			/* no, read from disk */
			int             cyl, head, sec;
			char           *trbuf;

			cyl = dblk / d->spc;
			head = (dblk % d->spc) / d->spt;
			sec = dblk % d->spt;
			nsec = d->spt - sec;

			if (cold) {
				/* transfer directly to buffer */
				trbuf = buf;
				if (nsec > num)
					nsec = num;
			} else {
				/* fill read-ahead buffer */
				trbuf = diskbuf;
				if (nsec > diskbuf_size)
					nsec = diskbuf_size;

				ra_dev = d->dev;
				ra_first = dblk;
				ra_end = dblk + nsec;
				diskbuf_user = &ra_dev;
			}

			if (biosread(d->dev, cyl, head, sec, nsec, trbuf)) {
				if (!cold)
					diskbuf_user = 0; /* mark invalid */
				return (-1);	/* XXX cannot output here if
						 * (cold) */
			}
		} else		/* can take blocks from end of read-ahead
				 * buffer */
			nsec = ra_end - dblk;

		if (!cold) {
			/* copy data from read-ahead to user buffer */
			if (nsec > num)
				nsec = num;
			bcopy(diskbuf + (dblk - ra_first) * BIOSDISK_SECSIZE,
			    buf, nsec * BIOSDISK_SECSIZE);
		}
		buf += nsec * BIOSDISK_SECSIZE;
		num -= nsec;
		dblk += nsec;
	}

	return (0);
}
