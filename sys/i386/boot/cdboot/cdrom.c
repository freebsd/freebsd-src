/*
 * Copyright © 1997 Pluto Technologies International, Inc.  Boulder CO
 * Copyright © 1997 interface business GmbH, Dresden.
 *	All rights reserved.
 *
 * This code was written by Jörg Wunsch, Dresden.
 * Direct comments to <joerg_wunsch@interface-business.de>.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * $Id: cdrom.c,v 1.2 1997/12/02 21:13:59 joerg Exp $
 */


#include "boot.h"

#include <isofs/cd9660/iso.h>

#define BLKSIZE	2048		/* CD-ROM data block size */
#define BIOSSEC	512		/* BIOS sector size */

#define CD2LBA(rba)	((rba) << 2) /* CD-ROM relative block to BIOS LBA */

u_int32_t sessionstart;

static struct iso_primary_descriptor pdesc;

static char *rootdirbuf;
static size_t rootdirsize;
static char xbuf[BLKSIZE];
static u_int32_t curblk, startblk, filesize, offset;

static int bread(u_int32_t rba, size_t nblks, void *buf);
static void badread(const char *msg, u_int32_t blkno);
static struct iso_directory_record *find(const char *path, int list_only);
static char *get_rr_name(struct iso_directory_record *dirp, size_t *len_ret);
static int iread(u_char *buf, size_t len,
		 void (*copyfun)(const void *src, void *dst, size_t size));

static struct daddrpacket dpkt = { 0x10 };

int
devopen(u_int32_t session)
{
	int rv;
	u_int32_t rootdirblk;
	struct iso_directory_record *rootdirp;

	if ((rv = bread(session + 16, 1, &pdesc)) != 0) {
		printf("Error reading primary ISO descriptor: %d\n", rv);
		return -1;
	}
	rootdirp = (struct iso_directory_record *)pdesc.root_directory_record;
	rootdirblk = isonum_733(rootdirp->extent);
	rootdirsize = isonum_733(rootdirp->size);

	/* just in case, round up */
	rootdirsize = (rootdirsize + BLKSIZE - 1) & ~(BLKSIZE - 1);

	if (rootdirbuf != NULL)
		free(rootdirbuf);
	if ((rootdirbuf = malloc(rootdirsize)) == 0) {
		printf("Cannot allocate memory for the root "
		       "directory buffer.\n");
		return -1;
	}
	if ((rv = bread(rootdirblk, rootdirsize / BLKSIZE, rootdirbuf))
	    != 0) {
		printf("Error reading root directory: %d\n", rv);
		return -1;
	}

	DPRINTF(("Root directory is 0x%x bytes @ %d\n",
		 rootdirsize, rootdirblk));

	return 0;
}

static int
bread(u_int32_t rba, size_t nblks, void *buf)
{
	int i, rv;

	for (i = 0, rv = -1; rv != 0 && i < 3; i++) {
		dpkt.nblocks = nblks * (BLKSIZE / BIOSSEC);
		dpkt.boffs = (u_int16_t)((int)buf & 0xffff);
		dpkt.bseg = BOOTSEG;
		dpkt.lba = CD2LBA(rba);

#ifdef DEBUG_VERBOSE
		DPRINTF(("Calling biosreadlba(%d blocks, lba %d) = ",
			 dpkt.nblocks, dpkt.lba));
#endif

		rv = biosreadlba(&dpkt);

#ifdef DEBUG_VERBOSE
		DPRINTF(("%d\n", rv));
#endif
	}
	return rv;
}


void
seek(u_int32_t offs)
{
	offset = offs;
}

static void
badread(const char *msg, u_int32_t blkno)
{
	printf("Error reading block %d from CD-ROM: %s\n",
	       blkno, msg);
}

static __inline size_t
minlen(size_t a, size_t b)
{
	return a < b? a: b;
}

/*
 * Internal form of read()/xread().
 */
static int
iread(u_char *buf, size_t len,
      void (*copyfun)(const void *src, void *dst, size_t size))
{
	u_int32_t newblk, ptr;
	size_t bsize;

	newblk = offset / BLKSIZE + startblk;

	if (newblk != curblk) {
		if (offset + len >= filesize) {
			badread("access beyond file limit", newblk);
			return -1;
		}
		if (bread(newblk, 1, xbuf)) {
			badread("BIOS read error", newblk);
			return -1;
		}
		curblk = newblk;
	}
	ptr = offset & (BLKSIZE - 1);
	if (ptr > 0) {
		/* initial short transfer */
		bsize = minlen(BLKSIZE - ptr, len);
		copyfun(xbuf + ptr, buf, bsize);
		buf += bsize;
		len -= bsize;
		offset += bsize;
	}
	for (; len > 0; len -= bsize) {
		bsize = minlen(len, BLKSIZE);
		newblk = offset / BLKSIZE + startblk;

		if (newblk != curblk) {
			if (offset + bsize > filesize) {
				badread("access beyond file limit", newblk);
				return -1;
			}
			if (bread(newblk, 1, xbuf)) {
				badread("BIOS read error", newblk);
				return -1;
			}
			curblk = newblk;
		}
		copyfun(xbuf, buf, bsize);
		buf += bsize;
		offset += bsize;
	}
	return 0;
}

int
read(u_char *buf, size_t len)
{
	DPRINTF(("read(0x%x, %d)\n", (int)buf, len));
	return iread(buf, len, bcopy);
}

int
xread(u_char *buf, size_t len)
{
	DPRINTF(("xread(0x%x, %d)\n", (int)buf, len));
	return iread(buf, len, pcpy);
}

static char *
get_rr_name(struct iso_directory_record *dirp, size_t *len_ret)
{
	struct rr_header {
		char	type[2];
		u_char	len;
		u_char	version;
	} *rrp;
	struct rr_nm_header {
		struct rr_header rrh;
		u_char	flags;
		char	name[0]; /* XXX -- using gcc extension */
	} *rrnmp;
	char *cp;

	cp = dirp->name + (u_char)dirp->name_len[0];
	/* round up to 16-bit boundary; ugly */
	cp = (char *)(((int)cp + 1) & ~1);
	rrp = (struct rr_header *)cp;

	if (rrp->type[0] != 'R' || rrp->type[1] != 'R') {
		DPRINTF(("no RR, "));
		return 0;
	}

	DPRINTF(("RR attribs: "));
	cp += rrp->len;
	while (cp - (char *)dirp <= (u_char)dirp->length[0]) {
		rrp = (struct rr_header *)cp;
		DPRINTF(("%c%c ", rrp->type[0], rrp->type[1]));
		if (rrp->type[0] == 'N' && rrp->type[1] == 'M') {
			rrnmp = (struct rr_nm_header *)rrp;
			*len_ret = rrp->len - sizeof(struct rr_nm_header);
			return rrnmp->name;
		}
		cp += rrp->len;
	}

	return 0;
}

static struct iso_directory_record *
find(const char *path, int list_only)
{
	struct iso_directory_record *dirp;
	char *ptr, *rrname;
	size_t len, entrylen;
	char namebuf[256];
	int i;
	int (*comp)(const char *, const char *);

	while (*path && *path == '/')
		path++;

	for (ptr = rootdirbuf, i = 1;
	     ptr < rootdirbuf + rootdirsize;
	     ptr += entrylen, i++) {
		dirp = (struct iso_directory_record *)ptr;
		entrylen = (u_char)dirp->length[0];
		len = (u_char)dirp->name_len[0];

		DPRINTF(("# %d: offset 0x%x, length 0x%x = %d, ",
			 i, (int)(ptr - rootdirbuf), entrylen, entrylen));

		if (entrylen == 0) {
			/*
			 * Dir entry of length 0.  That's the last
			 * entry in this block, advance to the next
			 * block (if any).  In case we get beyond the
			 * end of the directory, we'll fall off the
			 * loop due to the rootdirsize condition in
			 * the `for' statement.
			 */
			DPRINTF(("entrylen 0\n"));
			entrylen = (~((ptr - rootdirbuf) + BLKSIZE - 1))
				& (BLKSIZE - 1);
			continue;
		}
		if (len == 0) {
			DPRINTF(("name_len 0\n"));
			continue;
		}
		if (len == 1 &&
		    (dirp->name[0] == '\0' || dirp->name[1] == '\1')) {
			DPRINTF(("dot/dot-dot entry\n"));
			continue;
		}
		/* don't consider directories */
		if (dirp->flags[0] & 2) {
			DPRINTF(("directory\n"));
			continue;
		}
		rrname = get_rr_name(dirp, &len);
		comp = rrname? strcmp: strcasecmp;

		bcopy(rrname? rrname: dirp->name, namebuf, len);
		namebuf[len] = 0;
		DPRINTF(("name `%s'\n", namebuf));

		if (list_only) {
#ifndef DEBUG
			printf("%s ", namebuf);
#endif
		} else if (comp(path, namebuf) == 0)
			return dirp;
	}
#ifndef DEBUG
	if (list_only)
		printf("\n");
#endif
	return 0;
}

int
openrd(char *name)
{
	char *cp;
	const char *fname;
	u_int32_t oldsession;
	int session, list_only;
	struct iso_directory_record *dirp;

	session = 0;
	fname = name;

	/*
	 * We accept the following boot string:
	 *
	 * [@sessionstart] name
	 */
	for (cp = name; *cp; cp++)
		switch (*cp) {
		/* we don't support filenames with spaces */
		case ' ':	case '\t':
			break;

		case '@':
			if (session) {
				printf("Syntax error\n");
				return -1;
			}
			session++;
			oldsession = sessionstart;
			sessionstart = 0;
			break;

		case '0':	case '1':	case '2':
		case '3':	case '4':	case '5':
		case '6':	case '7':	case '8':
		case '9':
			if (session == 1) {
				sessionstart *= 10;
				sessionstart += *cp - '0';
			}
			break;

		default:
			if (session == 1) {
				session++;
				fname = cp;
			}
		}

	if (session && devopen(sessionstart) == -1) {
		(void)devopen(oldsession);
		sessionstart = oldsession;
	}
	if (session == 1)
		/* XXX no filename, only session arg */
		return -1;

	list_only = fname[0] == '?' && fname[1] == 0;

	DPRINTF(("Calling find(%s, %d):\n", fname, list_only));
	dirp = find(fname, list_only);
	DPRINTF(("find() returned 0x%x\n", (int)dirp));

	if (list_only)
		return -1;
	if (dirp == 0)
		return 1;

	startblk = isonum_733(dirp->extent);
	filesize = isonum_733(dirp->size);

	DPRINTF(("startblk = %d, filesize = %d\n", startblk, filesize));

	curblk = 0;	/* force a re-read, 0 is impossible file start */
	seek(0);

	return 0;
}
