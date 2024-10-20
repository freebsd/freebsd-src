/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2003-2007 Tim Kientzle
 * All rights reserved.
 */

#ifndef CPIO_H_INCLUDED
#define CPIO_H_INCLUDED

#include "cpio_platform.h"
#include <stdio.h>

/*
 * The internal state for the "cpio" program.
 *
 * Keeping all of the state in a structure like this simplifies memory
 * leak testing (at exit, anything left on the heap is suspect).  A
 * pointer to this structure is passed to most cpio internal
 * functions.
 */
struct cpio {
	/* Option parsing */
	const char	 *argument;

	/* Options */
	int		  add_filter; /* --uuencode */
	const char	 *filename;
	int		  mode; /* -i -o -p */
	int		  compress; /* -j, -y, or -z */
	const char	 *format; /* -H format */
	int		  bytes_per_block; /* -b block_size */
	int		  verbose;   /* -v */
	int		  dot;  /* -V */
	int		  quiet;   /* --quiet */
	int		  extract_flags; /* Flags for extract operation */
	const char	 *compress_program;
	int		  option_append; /* -A, only relevant for -o */
	int		  option_atime_restore; /* -a */
	int		  option_follow_links; /* -L */
	int		  option_link; /* -l */
	int		  option_list; /* -t */
	char		  option_null; /* --null */
	int		  option_numeric_uid_gid; /* -n */
	int		  option_pwb; /* -6 */
	int		  option_rename; /* -r */
	char		 *destdir;
	size_t		  destdir_len;
	size_t		  pass_destpath_alloc;
	char		 *pass_destpath;
	int		  uid_override;
	char		 *uname_override;
	int		  gid_override;
	char		 *gname_override;
	int		  day_first; /* true if locale prefers day/mon */
	const char	 *passphrase;

	/* If >= 0, then close this when done. */
	int		  fd;

	/* Miscellaneous state information */
	struct archive	 *archive;
	struct archive	 *archive_read_disk;
	int		  argc;
	char		**argv;
	int		  return_value; /* Value returned by main() */
	struct archive_entry_linkresolver *linkresolver;

	struct name_cache *uname_cache;
	struct name_cache *gname_cache;

	/* Work data. */
	struct archive   *matching;
	char		 *buff;
	size_t		  buff_size;
	char		 *ppbuff;
};

struct cpio_owner {
        int uid;
        int gid;
        char *uname;
        char *gname;
};

int owner_parse(const char *, struct cpio_owner *, const char **);

/* Fake short equivalents for long options that otherwise lack them. */
enum {
	OPTION_B64ENCODE = 1,
	OPTION_GRZIP,
	OPTION_INSECURE,
	OPTION_LRZIP,
	OPTION_LZ4,
	OPTION_LZMA,
	OPTION_LZOP,
	OPTION_PASSPHRASE,
	OPTION_NO_PRESERVE_OWNER,
	OPTION_PRESERVE_OWNER,
	OPTION_QUIET,
	OPTION_UUENCODE,
	OPTION_VERSION,
	OPTION_ZSTD,
};

int	cpio_getopt(struct cpio *cpio);

#endif
