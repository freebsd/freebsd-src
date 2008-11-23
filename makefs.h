/*	$NetBSD: makefs.h,v 1.14 2004/06/20 22:20:18 jmc Exp $	*/

/*
 * Copyright (c) 2001 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Luke Mewburn for Wasabi Systems, Inc.
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
 *      This product includes software developed for the NetBSD Project by
 *      Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef	_MAKEFS_H
#define	_MAKEFS_H

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#else
#define HAVE_STRUCT_STAT_ST_FLAGS 1
#define HAVE_STRUCT_STAT_ST_GEN 1
#define HAVE_STRUCT_STAT_ST_MTIMENSEC 1
#define HAVE_STRUCT_STATVFS_F_IOSIZE 1
#define HAVE_STRUCT_STAT_BIRTHTIME 1
#endif

#include <sys/stat.h>
#include <err.h>

/*
 * fsnode -
 *	a component of the tree; contains a filename, a pointer to
 *	fsinode, optional symlink name, and tree pointers
 *
 * fsinode - 
 *	equivalent to an inode, containing target file system inode number,
 *	refcount (nlink), and stat buffer
 *
 * A tree of fsnodes looks like this:
 *
 *	name	"."		"bin"		"netbsd"
 *	type	S_IFDIR		S_IFDIR		S_IFREG
 *	next 	  >		  >		NULL
 *	parent	NULL		NULL		NULL
 *	child	NULL		  v
 *
 *	name			"."		"ls"
 *	type			S_IFDIR		S_IFREG
 *	next			  >		NULL
 *	parent			  ^		^ (to "bin")
 *	child			NULL		NULL
 *
 * Notes:
 *	-   first always points to first entry, at current level, which
 *	    must be "." when the tree has been built; during build it may
 *	    not be if "." hasn't yet been found by readdir(2).
 */

enum fi_flags {
	FI_SIZED =	1<<0,		/* inode sized */
	FI_ALLOCATED =	1<<1,		/* fsinode->ino allocated */
	FI_WRITTEN =	1<<2,		/* inode written */
};

typedef struct {
	uint32_t	 ino;		/* inode number used on target fs */
	uint32_t	 nlink;		/* number of links to this entry */
	enum fi_flags	 flags;		/* flags used by fs specific code */
	struct stat	 st;		/* stat entry */
} fsinode;

typedef struct _fsnode {
	struct _fsnode	*parent;	/* parent (NULL if root) */
	struct _fsnode	*child;		/* child (if type == S_IFDIR) */
	struct _fsnode	*next;		/* next */
	struct _fsnode	*first;		/* first node of current level (".") */
	uint32_t	 type;		/* type of entry */
	fsinode		*inode;		/* actual inode data */
	char		*symlink;	/* symlink target */
	char		*name;		/* file name */
	int		flags;		/* misc flags */
} fsnode;

#define	FSNODE_F_HASSPEC	0x01	/* fsnode has a spec entry */

/*
 * fsinfo_t - contains various settings and parameters pertaining to
 * the image, including current settings, global options, and fs
 * specific options
 */
typedef struct {
		/* current settings */
	off_t	size;		/* total size */
	off_t	inodes;		/* number of inodes */
	uint32_t curinode;	/* current inode */

		/* image settings */
	int	fd;		/* file descriptor of image */
	void	*superblock;	/* superblock */
	int	onlyspec;	/* only add entries in specfile */


		/* global options */
	off_t	minsize;	/* minimum size image should be */
	off_t	maxsize;	/* maximum size image can be */
	off_t	freefiles;	/* free file entries to leave */
	int	freefilepc;	/* free file % */
	off_t	freeblocks;	/* free blocks to leave */
	int	freeblockpc;	/* free block % */
	int	needswap;	/* non-zero if byte swapping needed */
	int	sectorsize;	/* sector size */

		/* ffs specific options */
	int	bsize;		/* block size */
	int	fsize;		/* fragment size */
	int	cpg;		/* cylinders per group */
	int	cpgflg;		/* cpg was specified by user */
	int	density;	/* bytes per inode */
	int	ntracks;	/* number of tracks */
	int	nsectors;	/* number of sectors */
	int	rpm;		/* rpm */
	int	minfree;	/* free space threshold */
	int	optimization;	/* optimization (space or time) */
	int	maxcontig;	/* max contiguous blocks to allocate */
	int	rotdelay;	/* rotational delay between blocks */
	int	maxbpg;		/* maximum blocks per file in a cyl group */
	int	nrpos;		/* # of distinguished rotational positions */
	int	avgfilesize;	/* expected average file size */
	int	avgfpdir;	/* expected # of files per directory */
	int	version;	/* filesystem version (1 = FFS, 2 = UFS2) */
	int	maxbsize;	/* maximum extent size */
	int	maxblkspercg;	/* max # of blocks per cylinder group */
			/* XXX: support `old' file systems ? */
} fsinfo_t;


/*
 * option_t - contains option name, description, pointer to location to store
 * result, and range checks for the result. Used to simplify fs specific
 * option setting
 */
typedef struct {
	const char	*name;		/* option name */
	int		*value;		/* where to stuff the value */
	int		minimum;	/* minimum for value */
	int		maximum;	/* maximum for value */
	const char	*desc;		/* option description */
} option_t;


void		apply_specfile(const char *, const char *, fsnode *);
void		dump_fsnodes(const char *, fsnode *);
const char *	inode_type(mode_t);
int		set_option(option_t *, const char *, const char *);
fsnode *	walk_dir(const char *, fsnode *);

int		ffs_parse_opts(const char *, fsinfo_t *);
void		ffs_makefs(const char *, const char *, fsnode *, fsinfo_t *);



extern	u_int		debug;
extern	struct timespec	start_time;

/*
 * If -x is specified, we want to exclude nodes which do not appear
 * in the spec file.
 */
#define	FSNODE_EXCLUDE_P(opts, fsnode)	\
	((opts)->onlyspec != 0 && ((fsnode)->flags & FSNODE_F_HASSPEC) == 0)

#define	DEBUG_TIME			0x00000001
		/* debug bits 1..3 unused at this time */
#define	DEBUG_WALK_DIR			0x00000010
#define	DEBUG_WALK_DIR_NODE		0x00000020
#define	DEBUG_WALK_DIR_LINKCHECK	0x00000040
#define	DEBUG_DUMP_FSNODES		0x00000080
#define	DEBUG_DUMP_FSNODES_VERBOSE	0x00000100
#define	DEBUG_FS_PARSE_OPTS		0x00000200
#define	DEBUG_FS_MAKEFS			0x00000400
#define	DEBUG_FS_VALIDATE		0x00000800
#define	DEBUG_FS_CREATE_IMAGE		0x00001000
#define	DEBUG_FS_SIZE_DIR		0x00002000
#define	DEBUG_FS_SIZE_DIR_NODE		0x00004000
#define	DEBUG_FS_SIZE_DIR_ADD_DIRENT	0x00008000
#define	DEBUG_FS_POPULATE		0x00010000
#define	DEBUG_FS_POPULATE_DIRBUF	0x00020000
#define	DEBUG_FS_POPULATE_NODE		0x00040000
#define	DEBUG_FS_WRITE_FILE		0x00080000
#define	DEBUG_FS_WRITE_FILE_BLOCK	0x00100000
#define	DEBUG_FS_MAKE_DIRBUF		0x00200000
#define	DEBUG_FS_WRITE_INODE		0x00400000
#define	DEBUG_BUF_BREAD			0x00800000
#define	DEBUG_BUF_BWRITE		0x01000000
#define	DEBUG_BUF_GETBLK		0x02000000
#define	DEBUG_APPLY_SPECFILE		0x04000000
#define	DEBUG_APPLY_SPECENTRY		0x08000000


#define	TIMER_START(x)				\
	if (debug & DEBUG_TIME)			\
		gettimeofday(&(x), NULL)

#define	TIMER_RESULTS(x,d)				\
	if (debug & DEBUG_TIME) {			\
		struct timeval end, td;			\
		gettimeofday(&end, NULL);		\
		timersub(&end, &(x), &td);		\
		printf("%s took %ld.%06ld seconds\n",	\
		    (d), td.tv_sec, td.tv_usec);	\
	}


#ifndef	DEFAULT_FSTYPE
#define	DEFAULT_FSTYPE	"ffs"
#endif


/*
 *	ffs specific settings
 *	---------------------
 */

#define	FFS_EI		/* for opposite endian support in ffs headers */


#endif	/* _MAKEFS_H */
