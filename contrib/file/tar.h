/*
 * Copyright (c) Ian F. Darwin 1986-1995.
 * Software written by Ian F. Darwin and others;
 * maintained 1995-present by Christos Zoulas and others.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*
 * Header file for public domain tar (tape archive) program.
 *
 * @(#)tar.h 1.20 86/10/29	Public Domain.
 *
 * Created 25 August 1985 by John Gilmore, ihnp4!hoptoad!gnu.
 *
 * $Id: tar.h,v 1.9 2006/05/03 15:19:25 christos Exp $ # checkin only
 */

/*
 * Kludge for handling systems that cannot cope with multiple
 * external definitions of a variable.  In ONE routine (tar.c),
 * we #define TAR_EXTERN to null; here, we set it to "extern" if
 * it is not already set.
 */
#ifndef TAR_EXTERN
#define TAR_EXTERN extern
#endif

/*
 * Header block on tape.
 *
 * I'm going to use traditional DP naming conventions here.
 * A "block" is a big chunk of stuff that we do I/O on.
 * A "record" is a piece of info that we care about.
 * Typically many "record"s fit into a "block".
 */
#define	RECORDSIZE	512
#define	NAMSIZ	100
#define	TUNMLEN	32
#define	TGNMLEN	32

union record {
	char		charptr[RECORDSIZE];
	struct header {
		char	name[NAMSIZ];
		char	mode[8];
		char	uid[8];
		char	gid[8];
		char	size[12];
		char	mtime[12];
		char	chksum[8];
		char	linkflag;
		char	linkname[NAMSIZ];
		char	magic[8];
		char	uname[TUNMLEN];
		char	gname[TGNMLEN];
		char	devmajor[8];
		char	devminor[8];
	} header;
};

/* The checksum field is filled with this while the checksum is computed. */
#define	CHKBLANKS	"        "	/* 8 blanks, no null */

/* The magic field is filled with this if uname and gname are valid. */
#define	TMAGIC		"ustar"		/* 5 chars and a null */
#define	GNUTMAGIC	"ustar  "	/* 7 chars and a null */

/* The linkflag defines the type of file */
#define	LF_OLDNORMAL	'\0'		/* Normal disk file, Unix compat */
#define	LF_NORMAL	'0'		/* Normal disk file */
#define	LF_LINK		'1'		/* Link to previously dumped file */
#define	LF_SYMLINK	'2'		/* Symbolic link */
#define	LF_CHR		'3'		/* Character special file */
#define	LF_BLK		'4'		/* Block special file */
#define	LF_DIR		'5'		/* Directory */
#define	LF_FIFO		'6'		/* FIFO special file */
#define	LF_CONTIG	'7'		/* Contiguous file */
/* Further link types may be defined later. */

/*
 * Exit codes from the "tar" program
 */
#define	EX_SUCCESS	0		/* success! */
#define	EX_ARGSBAD	1		/* invalid args */
#define	EX_BADFILE	2		/* invalid filename */
#define	EX_BADARCH	3		/* bad archive */
#define	EX_SYSTEM	4		/* system gave unexpected error */


/*
 * Global variables
 */
TAR_EXTERN union record	*ar_block;	/* Start of block of archive */
TAR_EXTERN union record	*ar_record;	/* Current record of archive */
TAR_EXTERN union record	*ar_last;	/* Last+1 record of archive block */
TAR_EXTERN char		ar_reading;	/* 0 writing, !0 reading archive */
TAR_EXTERN int		blocking;	/* Size of each block, in records */
TAR_EXTERN int		blocksize;	/* Size of each block, in bytes */
TAR_EXTERN char		*ar_file;	/* File containing archive */
TAR_EXTERN char		*name_file;	/* File containing names to work on */
TAR_EXTERN char		*tar;		/* Name of this program */

/*
 * Flags from the command line
 */
TAR_EXTERN char	f_reblock;		/* -B */
TAR_EXTERN char	f_create;		/* -c */
TAR_EXTERN char	f_debug;		/* -d */
TAR_EXTERN char	f_sayblock;		/* -D */
TAR_EXTERN char	f_follow_links;		/* -h */
TAR_EXTERN char	f_ignorez;		/* -i */
TAR_EXTERN char	f_keep;			/* -k */
TAR_EXTERN char	f_modified;		/* -m */
TAR_EXTERN char	f_oldarch;		/* -o */
TAR_EXTERN char	f_use_protection;	/* -p */
TAR_EXTERN char	f_sorted_names;		/* -s */
TAR_EXTERN char	f_list;			/* -t */
TAR_EXTERN char	f_namefile;		/* -T */
TAR_EXTERN char	f_verbose;		/* -v */
TAR_EXTERN char	f_extract;		/* -x */
TAR_EXTERN char	f_compress;		/* -z */

/*
 * We now default to Unix Standard format rather than 4.2BSD tar format.
 * The code can actually produce all three:
 *	f_standard	ANSI standard
 *	f_oldarch	V7
 *	neither		4.2BSD
 * but we don't bother, since 4.2BSD can read ANSI standard format anyway.
 * The only advantage to the "neither" option is that we can cmp(1) our
 * output to the output of 4.2BSD tar, for debugging.
 */
#define		f_standard		(!f_oldarch)

/*
 * Structure for keeping track of filenames and lists thereof.
 */
struct name {
	struct name	*next;
	short		length;
	char		found;
	char		name[NAMSIZ+1];
};

TAR_EXTERN struct name	*namelist;	/* Points to first name in list */
TAR_EXTERN struct name	*namelast;	/* Points to last name in list */

TAR_EXTERN int		archive;	/* File descriptor for archive file */
TAR_EXTERN int		errors;		/* # of files in error */

/*
 *
 * Due to the next struct declaration, each routine that includes
 * "tar.h" must also include <sys/types.h>.  I tried to make it automatic,
 * but System V has no defines in <sys/types.h>, so there is no way of
 * knowing when it has been included.  In addition, it cannot be included
 * twice, but must be included exactly once.  Argghh!
 *
 * Thanks, typedef.  Thanks, USG.
 */
struct link {
	struct link	*next;
	dev_t		dev;
	ino_t		ino;
	short		linkcount;
	char		name[NAMSIZ+1];
};

TAR_EXTERN struct link	*linklist;	/* Points to first link in list */


/*
 * Error recovery stuff
 */
TAR_EXTERN char		read_error_flag;


#if 0
/*
 * Declarations of functions available to the world.
 */
/*LINTLIBRARY*/
#define	 annorec(stream, msg)	anno(stream, msg, 0)	/* Cur rec */
#define	annofile(stream, msg)	anno(stream, msg, 1)	/* Saved rec */
#endif
