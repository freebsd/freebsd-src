/* $Id: globals.h,v 1.1 1994/03/06 22:59:44 nate Exp $ */
#ifndef __GLOBALS_H__
#define __GLOBALS_H__
/*
 *  Remember to initialize the variable in globals.c if you want, and
 *  provide an alternative short name in globrename.h
 */
#include "globrename.h"

extern size_t _malloc_minchunk;

extern Word *_malloc_rover;
extern Word *_malloc_hiword;
extern Word *_malloc_loword;

extern size_t _malloc_sbrkunits;

extern size_t _malloc_totalavail;

extern Word *_malloc_mem;

extern int _malloc_tracing;	/* No tracing */
extern FILE *_malloc_statsfile;
extern char _malloc_statsbuf[];

extern int _malloc_leaktrace;

#ifdef PROFILESIZES
extern int _malloc_scount[];
#endif /* PROFILESIZES */

#ifdef DEBUG
/*
 *  0 or 1 means checking all pointers before using them. Reasonably
 *  thorough.  2 means check the entire heap on every call to
 *  malloc/free/realloc/memalign. (the rest call these)
 */
extern int _malloc_debugging;
#endif /* DEBUG */

extern univptr_t (* _malloc_memfunc) proto((size_t));

#endif /* __GLOBALS_H__ */ /* Do not add anything after this line */
