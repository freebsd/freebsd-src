/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
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

typedef struct {
	int		 dir;		/* base directory handle */
	char		 base[PATH_MAX + 1];	/* base directory path */
	char		*end;		/* pointer to NUL at end of path */
	char		 path[PATH_MAX];	/* target path */
} PATH_T;

extern PATH_T to;
extern bool Nflag, fflag, iflag, lflag, nflag, pflag, sflag, vflag;
extern volatile sig_atomic_t info;

__BEGIN_DECLS
int	copy_fifo(struct stat *, bool, bool);
int	copy_file(const FTSENT *, bool, bool);
int	copy_link(const FTSENT *, bool, bool);
int	copy_special(struct stat *, bool, bool);
int	setfile(struct stat *, int, bool);
int	preserve_dir_acls(const char *, const char *);
int	preserve_fd_acls(int, int);
void	usage(void) __dead2;
__END_DECLS

/*
 * The FreeBSD and Darwin kernels return ENOTCAPABLE when a path lookup
 * violates a RESOLVE_BENEATH constraint.  This results in confusing error
 * messages, so translate it to the more widely recognized EACCES.
 */
#ifdef ENOTCAPABLE
#define warn(...)							\
	warnc(errno == ENOTCAPABLE ? EACCES : errno, __VA_ARGS__)
#define err(rv, ...)							\
	errc(rv, errno == ENOTCAPABLE ? EACCES : errno, __VA_ARGS__)
#endif
