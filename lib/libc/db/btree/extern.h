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
 *	@(#)extern.h	8.10 (Berkeley) 7/20/94
 */

int	 __bt_close __P((DB *));
int	 __bt_cmp __P((BTREE *, const DBT *, EPG *));
int	 __bt_crsrdel __P((BTREE *, EPGNO *));
int	 __bt_defcmp __P((const DBT *, const DBT *));
size_t	 __bt_defpfx __P((const DBT *, const DBT *));
int	 __bt_delete __P((const DB *, const DBT *, u_int));
int	 __bt_dleaf __P((BTREE *, const DBT *, PAGE *, u_int));
int	 __bt_fd __P((const DB *));
int	 __bt_free __P((BTREE *, PAGE *));
int	 __bt_get __P((const DB *, const DBT *, DBT *, u_int));
PAGE	*__bt_new __P((BTREE *, pgno_t *));
void	 __bt_pgin __P((void *, pgno_t, void *));
void	 __bt_pgout __P((void *, pgno_t, void *));
int	 __bt_push __P((BTREE *, pgno_t, int));
int	 __bt_put __P((const DB *dbp, DBT *, const DBT *, u_int));
int	 __bt_ret __P((BTREE *, EPG *, DBT *, DBT *, DBT *, DBT *, int));
EPG	*__bt_search __P((BTREE *, const DBT *, int *));
int	 __bt_seq __P((const DB *, DBT *, DBT *, u_int));
void	 __bt_setcur __P((BTREE *, pgno_t, u_int));
int	 __bt_split __P((BTREE *, PAGE *,
	    const DBT *, const DBT *, int, size_t, u_int32_t));
int	 __bt_sync __P((const DB *, u_int));

int	 __ovfl_delete __P((BTREE *, void *));
int	 __ovfl_get __P((BTREE *, void *, size_t *, void **, size_t *));
int	 __ovfl_put __P((BTREE *, const DBT *, pgno_t *));

#ifdef DEBUG
void	 __bt_dnpage __P((DB *, pgno_t));
void	 __bt_dpage __P((PAGE *));
void	 __bt_dump __P((DB *));
#endif
#ifdef STATISTICS
void	 __bt_stat __P((DB *));
#endif
