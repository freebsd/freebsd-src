/*-
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *
 *	@(#)extern.h	8.3 (Berkeley) 4/16/94
 */

#include <sys/cdefs.h>

void	 brace_subst __P((char *, char **, char *, int));
void	*emalloc __P((unsigned int));
PLAN	*find_create __P((char ***));
int	 find_execute __P((PLAN *, char **));
PLAN	*find_formplan __P((char **));
PLAN	*not_squish __P((PLAN *));
PLAN	*or_squish __P((PLAN *));
PLAN	*paren_squish __P((PLAN *));
struct stat;
void	 printlong __P((char *, char *, struct stat *));
int	 queryuser __P((char **));

PLAN	*c_atime __P((char *));
PLAN	*c_ctime __P((char *));
PLAN	*c_delete __P((void));
PLAN	*c_depth __P((void));
PLAN	*c_exec __P((char ***, int));
PLAN	*c_execdir __P((char ***));
PLAN	*c_follow __P((void));
PLAN	*c_fstype __P((char *));
PLAN	*c_group __P((char *));
PLAN	*c_inum __P((char *));
PLAN	*c_links __P((char *));
PLAN	*c_ls __P((void));
PLAN	*c_name __P((char *));
PLAN	*c_newer __P((char *));
PLAN	*c_nogroup __P((void));
PLAN	*c_nouser __P((void));
PLAN	*c_path __P((char *));
PLAN	*c_perm __P((char *));
PLAN	*c_print __P((void));
PLAN	*c_print0 __P((void));
PLAN	*c_prune __P((void));
PLAN	*c_size __P((char *));
PLAN	*c_type __P((char *));
PLAN	*c_user __P((char *));
PLAN	*c_xdev __P((void));
PLAN	*c_openparen __P((void));
PLAN	*c_closeparen __P((void));
PLAN	*c_mtime __P((char *));
PLAN	*c_not __P((void));
PLAN	*c_or __P((void));

extern int ftsoptions, isdeprecated, isdepth, isoutput, isxargs;
