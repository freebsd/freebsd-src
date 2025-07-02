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
 *	@(#)extern.h	8.11 (Berkeley) 1/9/95
 */

#define __bt_close	__kdb2_bt_close
#define __bt_cmp	__kdb2_bt_cmp
#define __bt_crsrdel	__kdb2_bt_crsrdel
#define __bt_defcmp	__kdb2_bt_defcmp
#define __bt_defpfx	__kdb2_bt_defpfx
#define __bt_delete	__kdb2_bt_delete
#define __bt_dleaf	__kdb2_bt_deleaf
#define __bt_fd		__kdb2_bt_fd
#define __bt_free	__kdb2_bt_free
#define __bt_get	__kdb2_bt_get
#define __bt_new	__kdb2_bt_new
#define __bt_pgin	__kdb2_bt_pgin
#define __bt_pgout	__kdb2_bt_pgout
#define __bt_push	__kdb2_bt_push
#define __bt_put	__kdb2_bt_put
#define __bt_ret	__kdb2_bt_ret
#define __bt_search	__kdb2_bt_search
#define __bt_seq	__kdb2_bt_seq
#define __bt_setcur	__kdb2_bt_setcur
#define __bt_split	__kdb2_bt_split
#define __bt_sync	__kdb2_bt_sync
#define __ovfl_delete	__kdb2_ovfl_delete
#define __ovfl_get	__kdb2_ovfl_get
#define __ovfl_put	__kdb2_ovfl_put
#define __bt_dnpage	__kdb2_bt_dnpage
#define __bt_dmpage	__kdb2_bt_dmpage
#define __bt_dpage	__kdb2_bt_dpage
#define __bt_dump	__kdb2_bt_dump
#define __bt_stat	__kdb2_bt_stat
#define __bt_relink	__kdb2_bt_relink

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
PAGE	*__bt_new __P((BTREE *, db_pgno_t *));
void	 __bt_pgin __P((void *, db_pgno_t, void *));
void	 __bt_pgout __P((void *, db_pgno_t, void *));
int	 __bt_push __P((BTREE *, db_pgno_t, int));
int	 __bt_put __P((const DB *dbp, DBT *, const DBT *, u_int));
int	 __bt_ret __P((BTREE *, EPG *, DBT *, DBT *, DBT *, DBT *, int));
EPG	*__bt_search __P((BTREE *, const DBT *, int *));
int	 __bt_seq __P((const DB *, DBT *, DBT *, u_int));
void	 __bt_setcur __P((BTREE *, db_pgno_t, u_int));
int	 __bt_split __P((BTREE *, PAGE *,
	    const DBT *, const DBT *, int, size_t, u_int32_t));
int	 __bt_sync __P((const DB *, u_int));

int	 __ovfl_delete __P((BTREE *, void *));
int	 __ovfl_get __P((BTREE *, void *, size_t *, void **, size_t *));
int	 __ovfl_put __P((BTREE *, const DBT *, db_pgno_t *));

int	 __bt_dnpage __P((DB *, db_pgno_t));
int	 __bt_dpage __P((DB *, PAGE *));
int	 __bt_dmpage __P((PAGE *));
int	 __bt_dump __P((DB *));

int	 __bt_stat __P((DB *));

int	 __bt_relink __P((BTREE *, PAGE *));
