/*
 * sdbm - ndbm work-alike hashed database library
 * based on Per-Ake Larson's Dynamic Hashing algorithms. BIT 18 (1978).
 * author: oz@nexus.yorku.ca
 * status: public domain. 
 */
#define DBLKSIZ 4096
#define PBLKSIZ 1024
#define PAIRMAX 1008			/* arbitrary on PBLKSIZ-N */
#define SPLTMAX	10			/* maximum allowed splits */
					/* for a single insertion */
#ifdef VMS
#define DIRFEXT	".sdbm_dir"
#else
#define DIRFEXT	".dir"
#endif
#define PAGFEXT	".pag"

typedef struct {
	int dirf;		       /* directory file descriptor */
	int pagf;		       /* page file descriptor */
	int flags;		       /* status/error flags, see below */
	long maxbno;		       /* size of dirfile in bits */
	long curbit;		       /* current bit number */
	long hmask;		       /* current hash mask */
	long blkptr;		       /* current block for nextkey */
	int keyptr;		       /* current key for nextkey */
	long blkno;		       /* current page to read/write */
	long pagbno;		       /* current page in pagbuf */
	char pagbuf[PBLKSIZ];	       /* page file block buffer */
	long dirbno;		       /* current block in dirbuf */
	char dirbuf[DBLKSIZ];	       /* directory file block buffer */
} DBM;

#define DBM_RDONLY	0x1	       /* data base open read-only */
#define DBM_IOERR	0x2	       /* data base I/O error */

/*
 * utility macros
 */
#define sdbm_rdonly(db)		((db)->flags & DBM_RDONLY)
#define sdbm_error(db)		((db)->flags & DBM_IOERR)

#define sdbm_clearerr(db)	((db)->flags &= ~DBM_IOERR)  /* ouch */

#define sdbm_dirfno(db)	((db)->dirf)
#define sdbm_pagfno(db)	((db)->pagf)

typedef struct {
	char *dptr;
	int dsize;
} datum;

EXTCONST datum nullitem
#ifdef DOINIT
                        = {0, 0}
#endif
                                   ;

#if defined(__STDC__) || defined(__cplusplus) || defined(CAN_PROTOTYPE)
#define proto(p) p
#else
#define proto(p) ()
#endif

/*
 * flags to sdbm_store
 */
#define DBM_INSERT	0
#define DBM_REPLACE	1

/*
 * ndbm interface
 */
extern DBM *sdbm_open proto((char *, int, int));
extern void sdbm_close proto((DBM *));
extern datum sdbm_fetch proto((DBM *, datum));
extern int sdbm_delete proto((DBM *, datum));
extern int sdbm_store proto((DBM *, datum, datum, int));
extern datum sdbm_firstkey proto((DBM *));
extern datum sdbm_nextkey proto((DBM *));
extern int sdbm_exists proto((DBM *, datum));

/*
 * other
 */
extern DBM *sdbm_prep proto((char *, char *, int, int));
extern long sdbm_hash proto((char *, int));

#ifndef SDBM_ONLY
#define dbm_open sdbm_open
#define dbm_close sdbm_close
#define dbm_fetch sdbm_fetch
#define dbm_store sdbm_store
#define dbm_delete sdbm_delete
#define dbm_firstkey sdbm_firstkey
#define dbm_nextkey sdbm_nextkey
#define dbm_error sdbm_error
#define dbm_clearerr sdbm_clearerr
#endif

/* Most of the following is stolen from perl.h.  We don't include
   perl.h here because we just want the portability parts of perl.h,
   not everything else.
*/
#ifndef H_PERL  /* Include guard */
#include "embed.h"  /* Follow all the global renamings. */

/*
 * The following contortions are brought to you on behalf of all the
 * standards, semi-standards, de facto standards, not-so-de-facto standards
 * of the world, as well as all the other botches anyone ever thought of.
 * The basic theory is that if we work hard enough here, the rest of the
 * code can be a lot prettier.  Well, so much for theory.  Sorry, Henry...
 */

#include <errno.h>
#ifdef HAS_SOCKET
#   ifdef I_NET_ERRNO
#     include <net/errno.h>
#   endif
#endif

#if defined(__STDC__) || defined(_AIX) || defined(__stdc__) || defined(__cplusplus)
# define STANDARD_C 1
#endif

#include <stdio.h>
#include <ctype.h>
#include <setjmp.h>

#if defined(I_UNISTD)
#include <unistd.h>
#endif

#ifdef VMS
#  include <file.h>
#  include <unixio.h>
#endif

#ifdef I_SYS_PARAM
#   if !defined(MSDOS) && !defined(WIN32) && !defined(VMS)
#       ifdef PARAM_NEEDS_TYPES
#	    include <sys/types.h>
#       endif
#       include <sys/param.h>
#   endif
#endif

#ifndef _TYPES_		/* If types.h defines this it's easy. */
#   ifndef major		/* Does everyone's types.h define this? */
#	include <sys/types.h>
#   endif
#endif

#include <sys/stat.h>

#ifndef SEEK_SET
# ifdef L_SET
#  define SEEK_SET	L_SET
# else
#  define SEEK_SET	0  /* Wild guess. */
# endif
#endif

/* Use all the "standard" definitions? */
#if defined(STANDARD_C) && defined(I_STDLIB)
#   include <stdlib.h>
#endif /* STANDARD_C */

#define MEM_SIZE Size_t

/* This comes after <stdlib.h> so we don't try to change the standard
 * library prototypes; we'll use our own instead. */

#if defined(MYMALLOC) && !defined(PERL_POLLUTE_MALLOC)
#  define malloc  Perl_malloc
#  define calloc  Perl_calloc
#  define realloc Perl_realloc
#  define free    Perl_mfree

Malloc_t Perl_malloc proto((MEM_SIZE nbytes));
Malloc_t Perl_calloc proto((MEM_SIZE elements, MEM_SIZE size));
Malloc_t Perl_realloc proto((Malloc_t where, MEM_SIZE nbytes));
Free_t   Perl_mfree proto((Malloc_t where));
#endif /* MYMALLOC */

#ifdef I_STRING
#include <string.h>
#else
#include <strings.h>
#endif

#ifdef I_MEMORY
#include <memory.h>
#endif      

#ifdef __cplusplus
#define HAS_MEMCPY
#endif

#ifdef HAS_MEMCPY
#  if !defined(STANDARD_C) && !defined(I_STRING) && !defined(I_MEMORY)
#    ifndef memcpy
        extern char * memcpy proto((char*, char*, int));
#    endif
#  endif
#else
#   ifndef memcpy
#	ifdef HAS_BCOPY
#	    define memcpy(d,s,l) bcopy(s,d,l)
#	else
#	    define memcpy(d,s,l) my_bcopy(s,d,l)
#	endif
#   endif
#endif /* HAS_MEMCPY */

#ifdef HAS_MEMSET
#  if !defined(STANDARD_C) && !defined(I_STRING) && !defined(I_MEMORY)
#    ifndef memset
	extern char *memset proto((char*, int, int));
#    endif
#  endif
#  define memzero(d,l) memset(d,0,l)
#else
#   ifndef memzero
#	ifdef HAS_BZERO
#	    define memzero(d,l) bzero(d,l)
#	else
#	    define memzero(d,l) my_bzero(d,l)
#	endif
#   endif
#endif /* HAS_MEMSET */

#if defined(mips) && defined(ultrix) && !defined(__STDC__)
#   undef HAS_MEMCMP
#endif

#if defined(HAS_MEMCMP) && defined(HAS_SANE_MEMCMP)
#  if !defined(STANDARD_C) && !defined(I_STRING) && !defined(I_MEMORY)
#    ifndef memcmp
	extern int memcmp proto((char*, char*, int));
#    endif
#  endif
#  ifdef BUGGY_MSC
  #  pragma function(memcmp)
#  endif
#else
#   ifndef memcmp
	/* maybe we should have included the full embedding header... */
#	ifdef NO_EMBED
#	    define memcmp my_memcmp
#	else
#	    define memcmp Perl_my_memcmp
#	endif
#ifndef __cplusplus
	extern int memcmp proto((char*, char*, int));
#endif
#   endif
#endif /* HAS_MEMCMP */

#ifndef HAS_BCMP
#   ifndef bcmp
#	define bcmp(s1,s2,l) memcmp(s1,s2,l)
#   endif
#endif /* !HAS_BCMP */

#ifdef HAS_MEMCMP
#  define memNE(s1,s2,l) (memcmp(s1,s2,l))
#  define memEQ(s1,s2,l) (!memcmp(s1,s2,l))
#else
#  define memNE(s1,s2,l) (bcmp(s1,s2,l))
#  define memEQ(s1,s2,l) (!bcmp(s1,s2,l))
#endif

#ifdef I_NETINET_IN
#  ifdef VMS
#    include <in.h>
#  else
#    include <netinet/in.h>
#  endif
#endif

#endif /* Include guard */

