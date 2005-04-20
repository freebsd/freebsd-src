/* GNU tar Archive Format description.

   Copyright (C) 1988, 1989, 1991, 1992, 1993, 1994, 1995, 1996,
   1997, 2000, 2001 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 2, or (at your option) any later
   version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
   Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* If OLDGNU_COMPATIBILITY is not zero, tar produces archives which, by
   default, are readable by older versions of GNU tar.  This can be
   overriden by using --posix; in this case, POSIXLY_CORRECT in environment
   may be set for enforcing stricter conformance.  If OLDGNU_COMPATIBILITY
   is zero or undefined, tar will eventually produces archives which, by
   default, POSIX compatible; then either using --posix or defining
   POSIXLY_CORRECT enforces stricter conformance.

   This #define will disappear in a few years.  FP, June 1995.  */
#define OLDGNU_COMPATIBILITY 1

/* tar Header Block, from POSIX 1003.1-1990.  */

/* POSIX header.  */

struct posix_header
{				/* byte offset */
  char name[100];		/*   0 */
  char mode[8];			/* 100 */
  char uid[8];			/* 108 */
  char gid[8];			/* 116 */
  char size[12];		/* 124 */
  char mtime[12];		/* 136 */
  char chksum[8];		/* 148 */
  char typeflag;		/* 156 */
  char linkname[100];		/* 157 */
  char magic[6];		/* 257 */
  char version[2];		/* 263 */
  char uname[32];		/* 265 */
  char gname[32];		/* 297 */
  char devmajor[8];		/* 329 */
  char devminor[8];		/* 337 */
  char prefix[155];		/* 345 */
				/* 500 */
};

#define TMAGIC   "ustar"	/* ustar and a null */
#define TMAGLEN  6
#define TVERSION "00"		/* 00 and no null */
#define TVERSLEN 2

/* Values used in typeflag field.  */
#define REGTYPE	 '0'		/* regular file */
#define AREGTYPE '\0'		/* regular file */
#define LNKTYPE  '1'		/* link */
#define SYMTYPE  '2'		/* reserved */
#define CHRTYPE  '3'		/* character special */
#define BLKTYPE  '4'		/* block special */
#define DIRTYPE  '5'		/* directory */
#define FIFOTYPE '6'		/* FIFO special */
#define CONTTYPE '7'		/* reserved */

/* Bits used in the mode field, values in octal.  */
#define TSUID    04000		/* set UID on execution */
#define TSGID    02000		/* set GID on execution */
#define TSVTX    01000		/* reserved */
				/* file permissions */
#define TUREAD   00400		/* read by owner */
#define TUWRITE  00200		/* write by owner */
#define TUEXEC   00100		/* execute/search by owner */
#define TGREAD   00040		/* read by group */
#define TGWRITE  00020		/* write by group */
#define TGEXEC   00010		/* execute/search by group */
#define TOREAD   00004		/* read by other */
#define TOWRITE  00002		/* write by other */
#define TOEXEC   00001		/* execute/search by other */

/* tar Header Block, GNU extensions.  */

/* In GNU tar, SYMTYPE is for to symbolic links, and CONTTYPE is for
   contiguous files, so maybe disobeying the `reserved' comment in POSIX
   header description.  I suspect these were meant to be used this way, and
   should not have really been `reserved' in the published standards.  */

/* *BEWARE* *BEWARE* *BEWARE* that the following information is still
   boiling, and may change.  Even if the OLDGNU format description should be
   accurate, the so-called GNU format is not yet fully decided.  It is
   surely meant to use only extensions allowed by POSIX, but the sketch
   below repeats some ugliness from the OLDGNU format, which should rather
   go away.  Sparse files should be saved in such a way that they do *not*
   require two passes at archive creation time.  Huge files get some POSIX
   fields to overflow, alternate solutions have to be sought for this.  */

/* Descriptor for a single file hole.  */

struct sparse
{				/* byte offset */
  char offset[12];		/*   0 */
  char numbytes[12];		/*  12 */
				/*  24 */
};

/* Sparse files are not supported in POSIX ustar format.  For sparse files
   with a POSIX header, a GNU extra header is provided which holds overall
   sparse information and a few sparse descriptors.  When an old GNU header
   replaces both the POSIX header and the GNU extra header, it holds some
   sparse descriptors too.  Whether POSIX or not, if more sparse descriptors
   are still needed, they are put into as many successive sparse headers as
   necessary.  The following constants tell how many sparse descriptors fit
   in each kind of header able to hold them.  */

#define SPARSES_IN_EXTRA_HEADER  16
#define SPARSES_IN_OLDGNU_HEADER 4
#define SPARSES_IN_SPARSE_HEADER 21

/* The GNU extra header contains some information GNU tar needs, but not
   foreseen in POSIX header format.  It is only used after a POSIX header
   (and never with old GNU headers), and immediately follows this POSIX
   header, when typeflag is a letter rather than a digit, so signaling a GNU
   extension.  */

struct extra_header
{				/* byte offset */
  char atime[12];		/*   0 */
  char ctime[12];		/*  12 */
  char offset[12];		/*  24 */
  char realsize[12];		/*  36 */
  char longnames[4];		/*  48 */
  char unused_pad1[68];		/*  52 */
  struct sparse sp[SPARSES_IN_EXTRA_HEADER];
				/* 120 */
  char isextended;		/* 504 */
				/* 505 */
};

/* Extension header for sparse files, used immediately after the GNU extra
   header, and used only if all sparse information cannot fit into that
   extra header.  There might even be many such extension headers, one after
   the other, until all sparse information has been recorded.  */

struct sparse_header
{				/* byte offset */
  struct sparse sp[SPARSES_IN_SPARSE_HEADER];
				/*   0 */
  char isextended;		/* 504 */
				/* 505 */
};

/* The old GNU format header conflicts with POSIX format in such a way that
   POSIX archives may fool old GNU tar's, and POSIX tar's might well be
   fooled by old GNU tar archives.  An old GNU format header uses the space
   used by the prefix field in a POSIX header, and cumulates information
   normally found in a GNU extra header.  With an old GNU tar header, we
   never see any POSIX header nor GNU extra header.  Supplementary sparse
   headers are allowed, however.  */

struct oldgnu_header
{				/* byte offset */
  char unused_pad1[345];	/*   0 */
  char atime[12];		/* 345 */
  char ctime[12];		/* 357 */
  char offset[12];		/* 369 */
  char longnames[4];		/* 381 */
  char unused_pad2;		/* 385 */
  struct sparse sp[SPARSES_IN_OLDGNU_HEADER];
				/* 386 */
  char isextended;		/* 482 */
  char realsize[12];		/* 483 */
				/* 495 */
};

/* OLDGNU_MAGIC uses both magic and version fields, which are contiguous.
   Found in an archive, it indicates an old GNU header format, which will be
   hopefully become obsolescent.  With OLDGNU_MAGIC, uname and gname are
   valid, though the header is not truly POSIX conforming.  */
#define OLDGNU_MAGIC "ustar  "	/* 7 chars and a null */

/* The standards committee allows only capital A through capital Z for
   user-defined expansion.  */

/* This is a dir entry that contains the names of files that were in the
   dir at the time the dump was made.  */
#define GNUTYPE_DUMPDIR	'D'

/* Identifies the *next* file on the tape as having a long linkname.  */
#define GNUTYPE_LONGLINK 'K'

/* Identifies the *next* file on the tape as having a long name.  */
#define GNUTYPE_LONGNAME 'L'

/* This is the continuation of a file that began on another volume.  */
#define GNUTYPE_MULTIVOL 'M'

/* For storing filenames that do not fit into the main header.  */
#define GNUTYPE_NAMES 'N'

/* This is for sparse files.  */
#define GNUTYPE_SPARSE 'S'

/* This file is a tape/volume header.  Ignore it on extraction.  */
#define GNUTYPE_VOLHDR 'V'

/* tar Header Block, overall structure.  */

/* tar files are made in basic blocks of this size.  */
#define BLOCKSIZE 512

enum archive_format
{
  DEFAULT_FORMAT,		/* format to be decided later */
  V7_FORMAT,			/* old V7 tar format */
  OLDGNU_FORMAT,		/* GNU format as per before tar 1.12 */
  POSIX_FORMAT,			/* restricted, pure POSIX format */
  GNU_FORMAT			/* POSIX format with GNU extensions */
};

union block
{
  char buffer[BLOCKSIZE];
  struct posix_header header;
  struct extra_header extra_header;
  struct oldgnu_header oldgnu_header;
  struct sparse_header sparse_header;
};

/* End of Format description.  */
