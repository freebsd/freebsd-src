/*	$FreeBSD$ */
/*	$NetBSD: rf_debugMem.h,v 1.7 1999/09/05 01:58:11 oster Exp $	*/
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Daniel Stodolsky, Mark Holland
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/*
 * rf_debugMem.h -- memory leak debugging module
 *
 * IMPORTANT:  if you put the lock/unlock mutex stuff back in here, you
 *             need to take it out of the routines in debugMem.c
 *
 */

#ifndef _RF__RF_DEBUGMEM_H_
#define _RF__RF_DEBUGMEM_H_

#include <dev/raidframe/rf_alloclist.h>

#ifdef _KERNEL
#include <sys/types.h>
#include <sys/malloc.h>

#if defined(__FreeBSD__)
MALLOC_DECLARE(M_RAIDFRAME);
#endif

#define RF_Malloc(_p_, _size_, _cast_)                                      \
  {                                                                         \
     _p_ = _cast_ malloc((u_long)_size_, M_RAIDFRAME, M_NOWAIT | M_ZERO);   \
     if (_p_ == NULL) panic("out of memory\n");				    \
     if (rf_memDebug) rf_record_malloc(_p_, _size_, __LINE__, __FILE__);    \
  }

#define RF_MallocAndAdd(__p_, __size_, __cast_, __alist_)                   \
  {                                                                         \
     RF_Malloc(__p_, __size_, __cast_);                                     \
     if (__alist_) rf_AddToAllocList(__alist_, __p_, __size_);              \
  }

#define RF_Calloc(_p_, _nel_, _elsz_, _cast_)                               \
  {                                                                         \
     RF_Malloc( _p_, (_nel_) * (_elsz_), _cast_);                           \
  }

#define RF_CallocAndAdd(__p,__nel,__elsz,__cast,__alist)                    \
  {                                                                         \
     RF_Calloc(__p, __nel, __elsz, __cast);                                 \
     if (__alist) rf_AddToAllocList(__alist, __p, (__nel)*(__elsz));        \
  }

#define RF_Free(_p_, _sz_)                                                  \
  {                                                                         \
     free((void *)(_p_), M_RAIDFRAME);                                      \
     if (rf_memDebug) rf_unrecord_malloc(_p_, (u_int32_t) (_sz_));          \
  }

#endif				/* _KERNEL */

void    rf_record_malloc(void *p, int size, int line, char *filen);
void    rf_unrecord_malloc(void *p, int sz);
void    rf_print_unfreed(void);
int     rf_ConfigureDebugMem(RF_ShutdownList_t ** listp);

#endif				/* !_RF__RF_DEBUGMEM_H_ */
