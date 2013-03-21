/*-
 * Copyright (c) 2003-2007 Tim Kientzle
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/usr.bin/tar/bsdtar.h,v 1.37 2008/12/06 07:37:14 kientzle Exp $
 */

#include "bsdtar_platform.h"
#include <stdio.h>

#define	DEFAULT_BYTES_PER_BLOCK	(20*512)
#define ENV_READER_OPTIONS	"TAR_READER_OPTIONS"
#define ENV_WRITER_OPTIONS	"TAR_WRITER_OPTIONS"
#define IGNORE_WRONG_MODULE_NAME "__ignore_wrong_module_name__,"

struct creation_set;
/*
 * The internal state for the "bsdtar" program.
 *
 * Keeping all of the state in a structure like this simplifies memory
 * leak testing (at exit, anything left on the heap is suspect).  A
 * pointer to this structure is passed to most bsdtar internal
 * functions.
 */
struct bsdtar {
	/* Options */
	const char	 *filename; /* -f filename */
	char		 *pending_chdir; /* -C dir */
	const char	 *names_from_file; /* -T file */
	int		  bytes_per_block; /* -b block_size */
	int		  bytes_in_last_block; /* See -b handling. */
	int		  verbose;   /* -v */
	int		  extract_flags; /* Flags for extract operation */
	int		  readdisk_flags; /* Flags for read disk operation */
	int		  strip_components; /* Remove this many leading dirs */
	int		  gid;  /* --gid */
	const char	 *gname; /* --gname */
	int		  uid;  /* --uid */
	const char	 *uname; /* --uname */
	char		  mode; /* Program mode: 'c', 't', 'r', 'u', 'x' */
	char		  symlink_mode; /* H or L, per BSD conventions */
	char		  option_absolute_paths; /* -P */
	char		  option_chroot; /* --chroot */
	char		  option_fast_read; /* --fast-read */
	const char	 *option_options; /* --options */
	char		  option_interactive; /* -w */
	char		  option_no_owner; /* -o */
	char		  option_no_subdirs; /* -n */
	char		  option_numeric_owner; /* --numeric-owner */
	char		  option_null; /* --null */
	char		  option_stdout; /* -O */
	char		  option_totals; /* --totals */
	char		  option_unlink_first; /* -U */
	char		  option_warn_links; /* --check-links */
	char		  day_first; /* show day before month in -tv output */
	struct creation_set *cset;

	/* Option parser state */
	int		  getopt_state;
	char		 *getopt_word;

	/* If >= 0, then close this when done. */
	int		  fd;

	/* Miscellaneous state information */
	int		  argc;
	char		**argv;
	const char	 *argument;
	size_t		  gs_width; /* For 'list_item' in read.c */
	size_t		  u_width; /* for 'list_item' in read.c */
	uid_t		  user_uid; /* UID running this program */
	int		  return_value; /* Value returned by main() */
	char		  warned_lead_slash; /* Already displayed warning */
	char		  next_line_is_dir; /* Used for -C parsing in -cT */

	/*
	 * Data for various subsystems.  Full definitions are located in
	 * the file where they are used.
	 */
	struct archive		*diskreader;	/* for write.c */
	struct archive_entry_linkresolver *resolver; /* for write.c */
	struct archive_dir	*archive_dir;	/* for write.c */
	struct name_cache	*gname_cache;	/* for write.c */
	char			*buff;		/* for write.c */
	size_t			 buff_size;	/* for write.c */
	int			 first_fs;	/* for write.c */
	struct archive		*matching;	/* for matching.c */
	struct security		*security;	/* for read.c */
	struct name_cache	*uname_cache;	/* for write.c */
	struct siginfo_data	*siginfo;	/* for siginfo.c */
	struct substitution	*substitution;	/* for subst.c */
};

/* Fake short equivalents for long options that otherwise lack them. */
enum {
	OPTION_B64ENCODE = 1,
	OPTION_CHECK_LINKS,
	OPTION_CHROOT,
	OPTION_DISABLE_COPYFILE,
	OPTION_EXCLUDE,
	OPTION_FORMAT,
	OPTION_GID,
	OPTION_GNAME,
	OPTION_GRZIP,
	OPTION_HELP,
	OPTION_HFS_COMPRESSION,
	OPTION_INCLUDE,
	OPTION_KEEP_NEWER_FILES,
	OPTION_LRZIP,
	OPTION_LZIP,
	OPTION_LZMA,
	OPTION_LZOP,
	OPTION_NEWER_CTIME,
	OPTION_NEWER_CTIME_THAN,
	OPTION_NEWER_MTIME,
	OPTION_NEWER_MTIME_THAN,
	OPTION_NODUMP,
	OPTION_NOPRESERVE_HFS_COMPRESSION,
	OPTION_NO_SAME_OWNER,
	OPTION_NO_SAME_PERMISSIONS,
	OPTION_NULL,
	OPTION_NUMERIC_OWNER,
	OPTION_OLDER_CTIME,
	OPTION_OLDER_CTIME_THAN,
	OPTION_OLDER_MTIME,
	OPTION_OLDER_MTIME_THAN,
	OPTION_ONE_FILE_SYSTEM,
	OPTION_OPTIONS,
	OPTION_POSIX,
	OPTION_SAME_OWNER,
	OPTION_STRIP_COMPONENTS,
	OPTION_TOTALS,
	OPTION_UID,
	OPTION_UNAME,
	OPTION_USE_COMPRESS_PROGRAM,
	OPTION_UUENCODE,
	OPTION_VERSION
};

int	bsdtar_getopt(struct bsdtar *);
void	do_chdir(struct bsdtar *);
int	edit_pathname(struct bsdtar *, struct archive_entry *);
int	need_report(void);
int	pathcmp(const char *a, const char *b);
void	safe_fprintf(FILE *, const char *fmt, ...);
void	set_chdir(struct bsdtar *, const char *newdir);
const char *tar_i64toa(int64_t);
void	tar_mode_c(struct bsdtar *bsdtar);
void	tar_mode_r(struct bsdtar *bsdtar);
void	tar_mode_t(struct bsdtar *bsdtar);
void	tar_mode_u(struct bsdtar *bsdtar);
void	tar_mode_x(struct bsdtar *bsdtar);
void	usage(void);
int	yes(const char *fmt, ...);

#if defined(HAVE_REGEX_H) || defined(HAVE_PCREPOSIX_H)
void	add_substitution(struct bsdtar *, const char *);
int	apply_substitution(struct bsdtar *, const char *, char **, int, int);
void	cleanup_substitution(struct bsdtar *);
#endif

void		cset_add_filter(struct creation_set *, const char *);
void		cset_add_filter_program(struct creation_set *, const char *);
int		cset_auto_compress(struct creation_set *, const char *);
void		cset_free(struct creation_set *);
const char *	cset_get_format(struct creation_set *);
struct creation_set *cset_new(void);
int		cset_read_support_filter_program(struct creation_set *,
		    struct archive *);
void		cset_set_format(struct creation_set *, const char *);
int		cset_write_add_filters(struct creation_set *,
		    struct archive *, const void **);

