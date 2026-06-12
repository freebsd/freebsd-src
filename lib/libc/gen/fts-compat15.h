/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1989, 1993
 *      The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _FTS_COMPAT15_H_
#define _FTS_COMPAT15_H_

typedef struct {
	struct _ftsent15 *fts_cur;	/* current node */
	struct _ftsent15 *fts_child;	/* linked list of children */
	struct _ftsent15 **fts_array;	/* sort array */
	dev_t		 fts_dev;	/* starting device # */
	char		*fts_path;	/* path for this descent */
	int		 fts_rfd;	/* fd for root */
	__size_t	 fts_pathlen;	/* sizeof(path) */
	__size_t	 fts_nitems;	/* elements in the sort array */
	union {
		int	(*fts_compar)	/* compare function */
		    (const struct _ftsent15 * const *,
		    const struct _ftsent15 * const *);
#ifdef __BLOCKS__
		int	(^fts_compar_b)
		    (const struct _ftsent15 * const *,
		    const struct _ftsent15 * const *);
#else
		void	*fts_compar_b;
#endif
	};
	int		 fts_options;	/* fts_open options, global flags */
	void		*fts_clientptr;	/* thunk for sort function */
} FTS15;

typedef struct _ftsent15 {
	struct _ftsent15 *fts_cycle;	/* cycle node */
	struct _ftsent15 *fts_parent;	/* parent directory */
	struct _ftsent15 *fts_link;	/* next file in directory */
	long long	 fts_number;	/* local numeric value */
	void		*fts_pointer;	/* local address value */
	char		*fts_accpath;	/* access path */
	char		*fts_path;	/* root path */
	int		 fts_errno;	/* errno for this node */
	int		 fts_symfd;	/* fd for symlink */
	__size_t	 fts_pathlen;	/* strlen(fts_path) */
	__size_t	 fts_namelen;	/* strlen(fts_name) */
	__ino_t		 fts_ino;	/* inode */
	__dev_t		 fts_dev;	/* device */
	__nlink_t	 fts_nlink;	/* link count */
	long		 fts_level;	/* depth (-1 to N) */
	int		 fts_info;	/* user status for FTSENT structure */
	unsigned	 fts_flags;	/* private flags for FTSENT structure */
	int		 fts_instr;	/* fts_set() instructions */
	struct stat	*fts_statp;	/* stat(2) information */
	char		*fts_name;	/* file name */
	FTS15		*fts_fts;	/* back pointer to main FTS */
} FTSENT15;

FTSENT15	*freebsd15_fts_children(FTS15 *, int);
int		 freebsd15_fts_close(FTS15 *);
void		*freebsd15_fts_get_clientptr(FTS15 *);
#define		 freebsd15_fts_get_clientptr(fts) ((fts)->fts_clientptr)
FTS15		*freebsd15_fts_get_stream(FTSENT15 *);
#define		 freebsd15_fts_get_stream(ftsent) ((ftsent)->fts_fts)
FTS15		*freebsd15_fts_open(char * const *, int,
		    int (*)(const FTSENT15 * const *,
		    const FTSENT15 * const *));
FTSENT15	*freebsd15_fts_read(FTS15 *);
int		 freebsd15_fts_set(FTS15 *, FTSENT15 *, int);
void		 freebsd15_fts_set_clientptr(FTS15 *, void *);

#endif /* !_FTS_COMPAT15_H_ */
