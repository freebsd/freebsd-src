/*-
 * Copyright (c) 2003-2004 Tim Kientzle
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name(s) of the author(s) may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
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

#include <archive.h>
#include <stdio.h>

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
	const char	 *start_dir; /* -C dir */
	const char	 *names_from_file; /* -T file */
	int		  bytes_per_block; /* -b block_size */
	int		  verbose;   /* -v */
	int		  extract_flags; /* Flags for extract operation */
	char		  symlink_mode; /* H or L, per BSD conventions */
	char		  create_compression; /* j, y, or z */
	char		  option_absolute_paths; /* -P */
	char		  option_dont_traverse_mounts; /* -X */
	char		  option_fast_read; /* --fast-read */
	char		  option_honor_nodump; /* --nodump */
	char		  option_interactive; /* -w */
	char		  option_no_subdirs; /* -d */
	char		  option_stdout; /* -p */
	char		  option_unlink_first; /* -U */
	char		  option_warn_links; /* -l */

	/* If >= 0, then close this when done. */
	int		  fd;

	/* Miscellaneous state information */
	int		  argc;
	char		**argv;
	size_t		  gs_width; /* For 'list_item' in read.c */
	size_t		  u_width; /* for 'list_item' in read.c */
	char		 *user_uname; /* User running this program */
	uid_t		  user_uid; /* UID running this program */

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

const char	*bsdtar_progname(void);
void	bsdtar_errc(int _eval, int _code, const char *fmt, ...);
void	bsdtar_strmode(struct archive_entry *entry, char *bp);
void	bsdtar_warnc(int _code, const char *fmt, ...);
void	cleanup_exclusions(struct bsdtar *);
void	exclude(struct bsdtar *, const char *pattern);
int	excluded(struct bsdtar *, const char *pathname);
void	include(struct bsdtar *, const char *pattern);
void	safe_fprintf(FILE *, const char *fmt, ...);
void	tar_mode_c(struct bsdtar *bsdtar);
void	tar_mode_r(struct bsdtar *bsdtar);
void	tar_mode_t(struct bsdtar *bsdtar);
void	tar_mode_u(struct bsdtar *bsdtar);
void	tar_mode_x(struct bsdtar *bsdtar);
int	unmatched_inclusions(struct bsdtar *bsdtar);
void	usage(void);
int	yes(const char *fmt, ...);

