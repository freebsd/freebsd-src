/*
 * Mach Operating System
 * Copyright (c) 1992, 1991 Carnegie Mellon University
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 *
 *	from: Mach, Revision 2.2  92/04/04  11:36:34  rpd
 *	$Id: sys.c,v 1.12 1996/09/07 21:06:43 bde Exp $
 */

#include "boot.h"
#include <sys/dir.h>
#include <sys/reboot.h>

#ifdef 0
/* #define BUFSIZE 4096 */
#define BUFSIZE MAXBSIZE

char buf[BUFSIZE], fsbuf[SBSIZE], iobuf[MAXBSIZE];
#endif

static char biosdrivedigit;

#define BUFSIZE 8192
#define MAPBUFSIZE BUFSIZE
char buf[BUFSIZE], fsbuf[BUFSIZE], iobuf[BUFSIZE];

char mapbuf[MAPBUFSIZE];
int mapblock;

void
xread(char *addr, int size)
{
	int count = BUFSIZE;
	while (size > 0) {
		if (BUFSIZE > size)
			count = size;
		read(buf, count);
		pcpy(buf, addr, count);
		size -= count;
		addr += count;
	}
}

void
read(char *buffer, int count)
{
	int logno, off, size;
	int cnt2, bnum2;

	while (count) {
		off = blkoff(fs, poff);
		logno = lblkno(fs, poff);
		cnt2 = size = blksize(fs, &inode, logno);
		bnum2 = fsbtodb(fs, block_map(logno)) + boff;
		if (	(!off)  && (size <= count))
		{
			devread(buffer, bnum2, cnt2);
		}
		else
		{
			size -= off;
			if (size > count)
				size = count;
			devread(iobuf, bnum2, cnt2);
			bcopy(iobuf+off, buffer, size);
		}
		buffer += size;
		count -= size;
		poff += size;
	}
}

int
find(char *path)
{
	char *rest, ch;
	int block, off, loc, ino = ROOTINO;
	struct direct *dp;
	char list_only;

	list_only = (path[0] == '?' && path[1] == '\0');
loop:
	devread(iobuf, fsbtodb(fs, ino_to_fsba(fs, ino)) + boff, fs->fs_bsize);
	bcopy((void *)&((struct dinode *)iobuf)[ino % fs->fs_inopb],
	      (void *)&inode.i_din,
	      sizeof (struct dinode));
	if (!*path)
		return 1;
	while (*path == '/')
		path++;
	if (!inode.i_size || ((inode.i_mode&IFMT) != IFDIR))
		return 0;
	for (rest = path; (ch = *rest) && ch != '/'; rest++) ;
	*rest = 0;
	loc = 0;
	do {
		if (loc >= inode.i_size) {
			if (list_only) {
				putchar('\n');
				return -1;
			} else {
				return 0;
			}
		}
		if (!(off = blkoff(fs, loc))) {
			block = lblkno(fs, loc);
			devread(iobuf, fsbtodb(fs, block_map(block)) + boff,
				blksize(fs, &inode, block));
		}
		dp = (struct direct *)(iobuf + off);
		loc += dp->d_reclen;
		if (dp->d_ino && list_only)
			printf("%s ", dp->d_name);
	} while (!dp->d_ino || strcmp(path, dp->d_name));
	ino = dp->d_ino;
	*(path = rest) = ch;
	goto loop;
}


int
block_map(int file_block)
{
	int bnum;
	if (file_block < NDADDR)
		return(inode.i_db[file_block]);
	if ((bnum=fsbtodb(fs, inode.i_ib[0])+boff) != mapblock) {
		devread(mapbuf, bnum, fs->fs_bsize);
		mapblock = bnum;
	}
	return (((int *)mapbuf)[(file_block - NDADDR) % NINDIR(fs)]);
}


int
openrd(void)
{
	char **devp, *cp = name;
	int biosdrive, ret;

	/*******************************************************\
	* If bracket given look for preceding device name	*
	\*******************************************************/
	while (*cp && *cp!='(')
		cp++;
	if (!*cp)
	{
		cp = name;
	}
	else
	{
		/*
		 * Look for a BIOS drive number (a leading digit followed
		 * by a colon).
		 */
		biosdrivedigit = '\0';
		if (*(name + 1) == ':' && *name >= '0' && *name <= '9') {
			biosdrivedigit = *name;
			name += 2;
		}

		if (cp++ != name)
		{
			for (devp = devs; *devp; devp++)
				if (name[0] == (*devp)[0] &&
				    name[1] == (*devp)[1])
					break;
			if (!*devp)
			{
				printf("Unknown device\n");
				return 1;
			}
			maj = devp-devs;
		}
		/*******************************************************\
		* Look inside brackets for unit number, and partition	*
		\*******************************************************/
		/*
		 * Allow any valid digit as the unit number, as the BIOS
		 * will complain if the unit number is out of range.
		 * Restricting the range here prevents the possibilty of using
		 * BIOSes that support more than 2 units.
		 * XXX Bad values may cause strange errors, need to check if
		 * what happens when a value out of range is supplied.
		 */
		if (*cp >= '0' && *cp <= '9')
			unit = *cp++ - '0';
		if (!*cp || (*cp == ',' && !*++cp))
			return 1;
		if (*cp >= 'a' && *cp <= 'p')
			part = *cp++ - 'a';
		while (*cp && *cp++!=')') ;
		if (!*cp)
			return 1;
	}
	biosdrive = biosdrivedigit - '0';
	if (biosdrivedigit == '\0') {
		biosdrive = unit;
#if BOOT_HD_BIAS > 0
		/* XXX */
		if (maj == 4)
			biosdrive += BOOT_HD_BIAS;
#endif
	}
	switch(maj)
	{
	case 0:
	case 4:
		dosdev = biosdrive | 0x80;
		break;
	case 2:
		dosdev = biosdrive;
		break;
	default:
		printf("Unknown device\n");
		return 1;
	}
	printf("dosdev = %x, biosdrive = %d, unit = %d, maj = %d\n",
		dosdev, biosdrive, unit, maj);

	/***********************************************\
	* Now we know the disk unit and part,		*
	* Load disk info, (open the device)		*
	\***********************************************/
	if (devopen())
		return 1;

	/***********************************************\
	* Load Filesystem info (mount the device)	*
	\***********************************************/
	devread((char *)(fs = (struct fs *)fsbuf), SBLOCK + boff, SBSIZE);
	/***********************************************\
	* Find the actual FILE on the mounted device	*
	\***********************************************/
	ret = find(cp);
	if (ret == 0)
		return 1;
	if (ret < 0)
		return -1;
	poff = 0;
	name = cp;
	return 0;
}
