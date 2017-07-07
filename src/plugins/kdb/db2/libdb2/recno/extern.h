/*-
 * Copyright (c) 1991, 1993
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
 *	@(#)extern.h	8.3 (Berkeley) 6/4/94
 */

#include "../btree/extern.h"

#define __rec_close	__kdb2_rec_close
#define __rec_delete	__kdb2_rec_delete
#define __rec_dleaf	__kdb2_rec_dleaf
#define __rec_fd	__kdb2_rec_fd
#define __rec_fmap	__kdb2_rec_fmap
#define __rec_fout	__kdb2_rec_fout
#define __rec_fpipe	__kdb2_rec_fpipe
#define __rec_get	__kdb2_rec_get
#define __rec_iput	__kdb2_rec_iput
#define __rec_put	__kdb2_rec_put
#define __rec_ret	__kdb2_rec_ret
#define __rec_search	__kdb2_rec_search
#define __rec_seq	__kdb2_rec_seq
#define __rec_sync	__kdb2_rec_sync
#define __rec_vmap	__kdb2_rec_vmap
#define __rec_vout	__kdb2_rec_vout
#define __rec_vpipe	__kdb2_rec_vpipe

int	 __rec_close __P((DB *));
int	 __rec_delete __P((const DB *, const DBT *, u_int));
int	 __rec_dleaf __P((BTREE *, PAGE *, u_int32_t));
int	 __rec_fd __P((const DB *));
int	 __rec_fmap __P((BTREE *, recno_t));
int	 __rec_fout __P((BTREE *));
int	 __rec_fpipe __P((BTREE *, recno_t));
int	 __rec_get __P((const DB *, const DBT *, DBT *, u_int));
int	 __rec_iput __P((BTREE *, recno_t, const DBT *, u_int));
int	 __rec_put __P((const DB *dbp, DBT *, const DBT *, u_int));
int	 __rec_ret __P((BTREE *, EPG *, recno_t, DBT *, DBT *));
EPG	*__rec_search __P((BTREE *, recno_t, enum SRCHOP));
int	 __rec_seq __P((const DB *, DBT *, DBT *, u_int));
int	 __rec_sync __P((const DB *, u_int));
int	 __rec_vmap __P((BTREE *, recno_t));
int	 __rec_vout __P((BTREE *));
int	 __rec_vpipe __P((BTREE *, recno_t));
