/* $Id: globrename.h,v 1.1 1994/03/06 22:59:45 nate Exp $ */
#ifndef __GLOBALRENAME_H__
#define __GLOBALRENAME_H__
/*
 *  Renaming all external symbols that are internal to the malloc to be
 *  unique within 6 characters for machines whose linkers just can't keep
 *  up. We hope the cpp is smart enough - if not, get GNU cccp or the
 *  cpp that comes with X Windows Version 11 Release 3.
 */
#ifdef SHORTNAMES
#define _malloc_minchunk	__MAL1_minchunk

#define _malloc_rover		__MAL2_rover
#define _malloc_hiword		__MAL3_hiword
#define _malloc_loword		__MAL4_loword

#define _malloc_sbrkunits	__MAL5_sbrkunits

#define _malloc_totalavail	__MAL6_totalavail

#define _malloc_mem			__MAL7_mem

#define _malloc_tracing		__MAL8_tracing
#define _malloc_statsfile	__MAL9_statsfile
#define _malloc_statsbuf	__MALA_statsbuf

#define _malloc_leaktrace	__MALB_leaktrace

#ifdef PROFILESIZES
#define _malloc_scount		__MALC_scount
#endif /* PROFILESIZES */

#ifdef DEBUG
/*
 *  0 or 1 means checking all pointers before using them. Reasonably
 *  thorough.  2 means check the entire heap on every call to
 *  malloc/free/realloc/memalign. (the rest call these)
 */
#define _malloc_debugging	__MALD_debugging
#endif /* DEBUG */
#define _malloc_version		__MALE_version

#define _malloc_memfunc		__MALF_memfunc

#endif /* SHORTNAMES */ /* Do not add anything after this line */
#endif /* __GLOBALRENAME_H__ */ /* Do not add anything after this line */
