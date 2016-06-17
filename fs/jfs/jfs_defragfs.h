/*
 *   Copyright (c) International Business Machines Corp., 2000-2001
 *
 *   This program is free software;  you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or 
 *   (at your option) any later version.
 * 
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY;  without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 *   the GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program;  if not, write to the Free Software 
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
#ifndef	_H_JFS_DEFRAGFS
#define _H_JFS_DEFRAGFS

/*
 *	defragfs parameter list
 */
struct defragfs {
	uint flag;		/* 4: */
	u8 dev;			/* 1: */
	u8 pad[3];		/* 3: */
	s32 fileset;		/* 4: */
	u32 inostamp;		/* 4: */
	u32 ino;		/* 4: */
	u32 gen;		/* 4: */
	s64 xoff;		/* 8: */
	s64 old_xaddr;		/* 8: */
	s64 new_xaddr;		/* 8: */
	s32 xlen;		/* 4: */
};

/* plist flag */
#define DEFRAGFS_SYNC		0x80000000
#define DEFRAGFS_COMMIT		0x40000000
#define DEFRAGFS_RELOCATE	0x10000000

#define	INODE_TYPE		0x0000F000	/* IFREG or IFDIR */

#define EXTENT_TYPE		0x000000ff
#define DTPAGE			0x00000001
#define XTPAGE			0x00000002
#define DATAEXT			0x00000004
#define EAEXT			0x00000008

#endif				/* _H_JFS_DEFRAGFS */
