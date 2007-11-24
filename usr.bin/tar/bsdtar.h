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
 * $FreeBSD$
 */

#include "bsdtar_platform.h"
#include <stdio.h>

#define	DEFAULT_BYTES_PER_BLOCK	(20*512)

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
	const char	 *create_format; /* -F format */
	char		 *pending_chdir; /* -C dir */
	const char	 *names_from_file; /* -T file */
	time_t		  newer_ctime_sec; /* --newer/--newer-than */
	long		  newer_ctime_nsec; /* --newer/--newer-than */
	time_t		  newer_mtime_sec; /* --newer-mtime */
	long		  newer_mtime_nsec; /* --newer-mtime-than */
	int		  bytes_per_block; /* -b block_size */
	int		  verbose;   /* -v */
	int		  extract_flags; /* Flags for extract operation */
	int		  strip_components; /* Remove this many leading dirs */
	char		  mode; /* Program mode: 'c', 't', 'r', 'u', 'x' */
	char		  symlink_mode; /* H or L, per BSD conventions */
	char		  create_compression; /* j, y, or z */
	const char	 *compress_program;
	char		  option_absolute_paths; /* -P */
	char		  option_dont_traverse_mounts; /* --one-file-system */
	char		  option_fast_read; /* --fast-read */
	char		  option_honor_nodump; /* --nodump */
	char		  option_interactive; /* -w */
	char		  option_no_owner; /* -o */
	char		  option_no_subdirs; /* -n */
	char		  option_null; /* --null */
	char		  option_stdout; /* -O */
	char		  option_totals; /* --totals */
	char		  option_unlink_first; /* -U */
	char		  option_warn_links; /* --check-links */
	char		  day_first; /* show day before month in -tv output */

	/* If >= 0, then close this when done. */
	int		  fd;

	/* Miscellaneous state information */
	struct archive	 *archive;
	const char	 *progname;
	int		  argc;
	char		**argv;
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
	struct archive_dir	*archive_dir;	/* for write.c */
	struct name_cache	*gname_cache;	/* for write.c */
	struct links_cache	*links_cache;	/* for write.c */
	struct matching		*matching;	/* for matching.c */
	struct security		*security;	/* for read.c */
	struct name_cache	*uname_cache;	/* for write.c */
};

void	bsdtar_errc(struct bsdtar *, int _eval, int _code,
	    const char *fmt, ...);
void	bsdtar_strmode(struct archive_entry *entry, char *bp);
void	bsdtar_warnc(struct bsdtar *, int _code, const char *fmt, ...);
void	cleanup_exclusions(struct bsdtar *);
void	do_chdir(struct bsdtar *);
int	edit_pathname(struct bsdtar *, struct archive_entry *);
int	exclude(struct bsdtar *, const char *pattern);
int	exclude_from_file(struct bsdtar *, const char *pathname);
int	excluded(struct bsdtar *, const char *pathname);
int	include(struct bsdtar *, const char *pattern);
int	include_from_file(struct bsdtar *, const char *pathname);
int	pathcmp(const char *a, const char *b);
int	process_lines(struct bsdtar *bsdtar, const char *pathname,
	    int (*process)(struct bsdtar *, const char *));
void	safe_fprintf(FILE *, const char *fmt, ...);
void	set_chdir(struct bsdtar *, const char *newdir);
void	tar_mode_c(struct bsdtar *bsdtar);
void	tar_mode_r(struct bsdtar *bsdtar);
void	tar_mode_t(struct bsdtar *bsdtar);
void	tar_mode_u(struct bsdtar *bsdtar);
void	tar_mode_x(struct bsdtar *bsdtar);
int	unmatched_inclusions(struct bsdtar *bsdtar);
void	usage(struct bsdtar *);
int	yes(const char *fmt, ...);

