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

/* Data for exclusion/inclusion handling: defined in matching.c */
struct matching;
struct links_entry;
struct archive_dir_entry;

/*
 * The internal state for the "bsdtar" program.  This is registered
 * with the 'archive' structure so that this information will be
 * available to the read/write callbacks.
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
	char		  option_warn_links; /* -l */

	/* If >= 0, then close this when done. */
	int		  fd;

	/* Miscellaneous state information */
	size_t		  u_width; /* for 'list_item' */
	size_t		  gs_width; /* For 'list_item' */
	char		 *user_uname; /* User running this program */
	uid_t		  user_uid; /* UID running this program */
	int		  argc;
	char		**argv;

	struct matching	 *matching;

	struct links_entry	*links_head;
	struct archive_dir_entry *archive_dir_head, *archive_dir_tail;

	/* An arbitrary prime number. */
	#define bsdtar_hash_size 71
	/* A simple hash of uid/uname for caching uname lookups. */
	struct {
		uid_t uid;
		char *uname;
	} uname_lookup[bsdtar_hash_size];

	/* A simple hash of gid/gname for caching gname lookups. */
	struct {
		gid_t gid;
		char *gname;
	} gname_lookup[bsdtar_hash_size];
};

const char	*bsdtar_progname(void);
void	bsdtar_errc(int _eval, int _code, const char *fmt, ...);
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

