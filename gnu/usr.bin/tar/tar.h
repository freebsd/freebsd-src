/* Declarations for tar archives.
   Copyright (C) 1988, 1992, 1993 Free Software Foundation

This file is part of GNU Tar.

GNU Tar is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU Tar is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU Tar; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* major() and minor() macros (among other things) defined here for hpux */
#ifdef hpux
#include <sys/mknod.h>
#endif

/*
 * Kludge for handling systems that can't cope with multiple
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
#define	NAMSIZ		100
#define	TUNMLEN		32
#define	TGNMLEN		32
#define SPARSE_EXT_HDR  21
#define SPARSE_IN_HDR	4

struct sparse
  {
    char offset[12];
    char numbytes[12];
  };

struct sp_array
  {
    int offset;
    int numbytes;
  };

union record
  {
    char charptr[RECORDSIZE];
    struct header
      {
	char arch_name[NAMSIZ];
	char mode[8];
	char uid[8];
	char gid[8];
	char size[12];
	char mtime[12];
	char chksum[8];
	char linkflag;
	char arch_linkname[NAMSIZ];
	char magic[8];
	char uname[TUNMLEN];
	char gname[TGNMLEN];
	char devmajor[8];
	char devminor[8];
	/* these following fields were added by JF for gnu */
	/* and are NOT standard */
	char atime[12];
	char ctime[12];
	char offset[12];
	char longnames[4];
#ifdef NEEDPAD
	char pad;
#endif
	struct sparse sp[SPARSE_IN_HDR];
	char isextended;
	char realsize[12];	/* true size of the sparse file */
	/* char	ending_blanks[12];*//* number of nulls at the
	   end of the file, if any */
      }
    header;
    struct extended_header
      {
	struct sparse sp[21];
	char isextended;
      }
    ext_hdr;
  };

/* The checksum field is filled with this while the checksum is computed. */
#define	CHKBLANKS	"        "	/* 8 blanks, no null */

/* The magic field is filled with this if uname and gname are valid. */
#define	TMAGIC		"ustar  "	/* 7 chars and a null */

/* The linkflag defines the type of file */
#define	LF_OLDNORMAL	'\0'	/* Normal disk file, Unix compat */
#define	LF_NORMAL	'0'	/* Normal disk file */
#define	LF_LINK		'1'	/* Link to previously dumped file */
#define	LF_SYMLINK	'2'	/* Symbolic link */
#define	LF_CHR		'3'	/* Character special file */
#define	LF_BLK		'4'	/* Block special file */
#define	LF_DIR		'5'	/* Directory */
#define	LF_FIFO		'6'	/* FIFO special file */
#define	LF_CONTIG	'7'	/* Contiguous file */
/* Further link types may be defined later. */

/* Note that the standards committee allows only capital A through
   capital Z for user-defined expansion.  This means that defining something
   as, say '8' is a *bad* idea. */
#define LF_DUMPDIR	'D'	/* This is a dir entry that contains
					   the names of files that were in
					   the dir at the time the dump
					   was made */
#define LF_LONGLINK	'K'	/* Identifies the NEXT file on the tape
					   as having a long linkname */
#define LF_LONGNAME	'L'	/* Identifies the NEXT file on the tape
					   as having a long name. */
#define LF_MULTIVOL	'M'	/* This is the continuation
					   of a file that began on another
					   volume */
#define LF_NAMES	'N'	/* For storing filenames that didn't
					   fit in 100 characters */
#define LF_SPARSE	'S'	/* This is for sparse files */
#define LF_VOLHDR	'V'	/* This file is a tape/volume header */
/* Ignore it on extraction */

/*
 * Exit codes from the "tar" program
 */
#define	EX_SUCCESS	0	/* success! */
#define	EX_ARGSBAD	1	/* invalid args */
#define	EX_BADFILE	2	/* invalid filename */
#define	EX_BADARCH	3	/* bad archive */
#define	EX_SYSTEM	4	/* system gave unexpected error */
#define EX_BADVOL	5	/* Special error code means
				   Tape volume doesn't match the one
				   specified on the command line */

/*
 * Global variables
 */
TAR_EXTERN union record *ar_block;	/* Start of block of archive */
TAR_EXTERN union record *ar_record;	/* Current record of archive */
TAR_EXTERN union record *ar_last;	/* Last+1 record of archive block */
TAR_EXTERN char ar_reading;	/* 0 writing, !0 reading archive */
TAR_EXTERN int blocking;	/* Size of each block, in records */
TAR_EXTERN int blocksize;	/* Size of each block, in bytes */
TAR_EXTERN char *info_script;	/* Script to run at end of each tape change */
TAR_EXTERN char *name_file;	/* File containing names to work on */
TAR_EXTERN char filename_terminator;	/* \n or \0. */
TAR_EXTERN char *tar;		/* Name of this program */
TAR_EXTERN struct sp_array *sparsearray;	/* Pointer to the start of the scratch space */
TAR_EXTERN int sp_array_size;	/* Initial size of the sparsearray */
TAR_EXTERN int tot_written;	/* Total written to output */
TAR_EXTERN struct re_pattern_buffer
 *label_pattern;		/* compiled regex for extract label */
TAR_EXTERN char **ar_files;	/* list of tape drive names */
TAR_EXTERN int n_ar_files;	/* number of tape drive names */
TAR_EXTERN int cur_ar_file;	/* tape drive currently being used */
TAR_EXTERN int ar_files_len;	/* malloced size of ar_files */
TAR_EXTERN char *current_file_name, *current_link_name;

/*
 * Flags from the command line
 */
TAR_EXTERN int cmd_mode;
#define CMD_NONE	0
#define CMD_CAT		1	/* -A */
#define CMD_CREATE	2	/* -c */
#define CMD_DIFF	3	/* -d */
#define CMD_APPEND	4	/* -r */
#define CMD_LIST	5	/* -t */
#define CMD_UPDATE	6	/* -u */
#define CMD_EXTRACT	7	/* -x */
#define CMD_DELETE	8	/* -D */
#define CMD_VERSION	9	/* --version */


TAR_EXTERN int f_reblock;	/* -B */
TAR_EXTERN int f_dironly;	/* -n ... PH */
TAR_EXTERN int f_run_script_at_end;	/* -F */
TAR_EXTERN int f_gnudump;	/* -G */
TAR_EXTERN int f_follow_links;	/* -h */
TAR_EXTERN int f_ignorez;	/* -i */
TAR_EXTERN int f_keep;		/* -k */
TAR_EXTERN int f_startfile;	/* -K */
TAR_EXTERN int f_local_filesys;	/* -l */
TAR_EXTERN int tape_length;	/* -L */
TAR_EXTERN int f_modified;	/* -m */
TAR_EXTERN int f_multivol;	/* -M */
TAR_EXTERN int f_new_files;	/* -N */
TAR_EXTERN int f_oldarch;	/* -o */
TAR_EXTERN int f_exstdout;	/* -O */
TAR_EXTERN int f_use_protection;/* -p */
TAR_EXTERN int f_absolute_paths;/* -P */
TAR_EXTERN int f_sayblock;	/* -R */
TAR_EXTERN int f_sorted_names;	/* -s */
TAR_EXTERN int f_sparse_files;	/* -S  ... JK */
TAR_EXTERN int f_namefile;	/* -T */
TAR_EXTERN int f_verbose;	/* -v */
TAR_EXTERN char *f_volhdr;	/* -V */
TAR_EXTERN int f_confirm;	/* -w */
TAR_EXTERN int f_verify;	/* -W */
TAR_EXTERN int f_exclude;	/* -X */
TAR_EXTERN char *f_compressprog;	/* -z and -Z */
TAR_EXTERN int f_do_chown;	/* --do-chown */
TAR_EXTERN int f_totals;	/* --totals */
TAR_EXTERN int f_remove_files;	/* --remove-files */
TAR_EXTERN int f_ignore_failed_read;	/* --ignore-failed-read */
TAR_EXTERN int f_checkpoint;	/* --checkpoint */
TAR_EXTERN int f_show_omitted_dirs;	/* --show-omitted-dirs */
TAR_EXTERN char *f_volno_file;	/* --volno-file */
TAR_EXTERN int f_force_local;	/* --force-local */
TAR_EXTERN int f_atime_preserve;/* --atime-preserve */
TAR_EXTERN int f_compress_block; /* --compress-block */
TAR_EXTERN int f_unlink;	/* --unlink */
TAR_EXTERN int f_fast_read;	/* --fast-read */

/*
 * We default to Unix Standard format rather than 4.2BSD tar format.
 * The code can actually produce all three:
 *	f_standard	ANSI standard
 *	f_oldarch	V7
 *	neither		4.2BSD
 * but we don't bother, since 4.2BSD can read ANSI standard format anyway.
 * The only advantage to the "neither" option is that we can cmp our
 * output to the output of 4.2BSD tar, for debugging.
 */
#define		f_standard		(!f_oldarch)

/*
 * Structure for keeping track of filenames and lists thereof.
 */
struct name
  {
    struct name *next;
    short length;		/* cached strlen(name) */
    char found;			/* A matching file has been found */
    char firstch;		/* First char is literally matched */
    char regexp;		/* This name is a regexp, not literal */
    char *change_dir;		/* JF set with the -C option */
    char *dir_contents;		/* JF for f_gnudump */
    char fake;			/* dummy entry */
    char name[1];
  };

TAR_EXTERN struct name *namelist;	/* Points to first name in list */
TAR_EXTERN struct name *namelast;	/* Points to last name in list */

TAR_EXTERN int archive;		/* File descriptor for archive file */
TAR_EXTERN int errors;		/* # of files in error */

TAR_EXTERN char *gnu_dumpfile;

/*
 * Error recovery stuff
 */
TAR_EXTERN char read_error_flag;

/*
 * global boolean, see name_match in tar.c
 */
extern int	nlpsfreed;

/*
 * Declarations of functions available to the world.
 */
union record *findrec ();
void userec ();
union record *endofrecs ();
void anno ();

#if defined (HAVE_VPRINTF) && __STDC__
void msg (char *,...);
void msg_perror (char *,...);
#else
void msg ();
void msg_perror ();
#endif
