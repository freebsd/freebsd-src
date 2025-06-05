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
 *	@(#)extern.h	8.8 (Berkeley) 11/7/95
 */

#define __add_bigpage		__kdb2_add_bigpage
#define __add_ovflpage		__kdb2_add_ovflpage
#define __addel			__kdb2_addel
#define __alloc_tmp		__kdb2_alloc_tmp
#define __big_delete		__kdb2_big_delete
#define __big_insert		__kdb2_big_insert
#define __big_keydata		__kdb2_big_keydata
#define __big_return		__kdb2_big_return
#define __call_hash		__kdb2_call_hash
#define __cursor_creat		__kdb2_cursor_creat
#define __delete_page		__kdb2_delete_page
#define __delpair		__kdb2_delpair
#define __expand_table		__kdb2_expand_table
#define __find_bigpair		__kdb2_find_bigpair
#define __free_ovflpage		__kdb2_free_ovflpage
#define __get_bigkey		__kdb2_get_bigkey
#define __get_buf		__kdb2_get_buf
#define __get_item		__kdb2_get_item
#define __get_item_done		__kdb2_get_item_done
#define __get_item_first	__kdb2_get_item_first
#define __get_item_next		__kdb2_get_item_next
#define __get_item_reset	__kdb2_get_item_reset
#define __get_page		__kdb2_get_page
#define __ibitmap		__kdb2_ibitmap
#define __log2			__kdb2_log2
#define __new_page		__kdb2_new_page
#define __pgin_routine		__kdb2_pgin_routine
#define __pgout_routine		__kdb2_pgout_routine
#define __put_buf		__kdb2_put_buf
#define __put_page		__kdb2_put_page
#define __reclaim_tmp		__kdb2_reclaim_tmp
#define __split_page		__kdb2_split_page

PAGE16	 *__add_bigpage __P((HTAB *, PAGE16 *, indx_t, const u_int8_t));
PAGE16	 *__add_ovflpage __P((HTAB *, PAGE16 *));
int32_t	  __addel __P((HTAB *, ITEM_INFO *,
		const DBT *, const DBT *, u_int32_t, const u_int8_t));
u_int32_t __alloc_tmp __P((HTAB*));
int32_t	  __big_delete __P((HTAB *, PAGE16 *, indx_t));
int32_t	  __big_insert __P((HTAB *, PAGE16 *, const DBT *, const DBT *));
int32_t	  __big_keydata __P((HTAB *, PAGE16 *, DBT *, DBT *, int32_t));
int32_t	  __big_return __P((HTAB *, ITEM_INFO *, DBT *, int32_t));
u_int32_t __call_hash __P((HTAB *, int8_t *, int32_t));
CURSOR	 *__cursor_creat __P((const DB *));
int32_t	  __delete_page __P((HTAB *, PAGE16 *, int32_t));
int32_t	  __delpair __P((HTAB *, CURSOR *, ITEM_INFO *));
int32_t	  __expand_table __P((HTAB *));
int32_t	  __find_bigpair __P((HTAB *, CURSOR *, int8_t *, int32_t));
void	  __free_ovflpage __P((HTAB *, PAGE16 *));
int32_t	  __get_bigkey __P((HTAB *, PAGE16 *, indx_t, DBT *));
PAGE16	 *__get_buf __P((HTAB *, u_int32_t, int32_t));
u_int32_t __get_item __P((HTAB *, CURSOR *, DBT *, DBT *, ITEM_INFO *));
u_int32_t __get_item_done __P((HTAB *, CURSOR *));
u_int32_t __get_item_first __P((HTAB *, CURSOR *, DBT *, DBT *, ITEM_INFO *));
u_int32_t __get_item_next __P((HTAB *, CURSOR *, DBT *, DBT *, ITEM_INFO *));
u_int32_t __get_item_reset __P((HTAB *, CURSOR *));
PAGE16	 *__get_page __P((HTAB *, u_int32_t, int32_t));
int32_t	  __ibitmap __P((HTAB *, int32_t, int32_t, int32_t));
u_int32_t __log2 __P((u_int32_t));
int32_t	  __new_page __P((HTAB *, u_int32_t, int32_t));
void	  __pgin_routine __P((void *, db_pgno_t, void *));
void	  __pgout_routine __P((void *, db_pgno_t, void *));
u_int32_t __put_buf __P((HTAB *, PAGE16 *, u_int32_t));
int32_t	  __put_page __P((HTAB *, PAGE16 *, int32_t, int32_t));
void	  __reclaim_tmp __P((HTAB *));
int32_t	  __split_page __P((HTAB *, u_int32_t, u_int32_t));

/* Default hash routine. */
extern u_int32_t (*__default_hash) __P((const void *, size_t));

#ifdef HASH_STATISTICS
extern long hash_accesses, hash_bigpages, hash_collisions, hash_expansions;
extern long hash_overflow;
#endif
