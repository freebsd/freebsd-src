/*
 * $Id: pam_malloc.h,v 1.2 2000/12/04 19:02:34 baggins Exp $
 */

/*
 * This file (via the use of macros) defines a wrapper for the malloc
 * family of calls. It logs where the memory was requested and also
 * where it was free()'d and keeps a list of currently requested memory.
 *
 * It is hoped that it will provide some help in locating memory leaks.
 */

#ifndef PAM_MALLOC_H
#define PAM_MALLOC_H

/* these are the macro definitions for the stdlib.h memory functions */

#define malloc(s)      pam_malloc(s,__FILE__,__FUNCTION__,__LINE__)
#define calloc(n,s)    pam_calloc(n,s,__FILE__,__FUNCTION__,__LINE__)
#define free(x)        pam_free(x,__FILE__,__FUNCTION__,__LINE__)
/* #define memalign(a,s)  pam_memalign(a,s,__FILE__,__FUNCTION__,__LINE__) */
#define realloc(x,s)   pam_realloc(x,s,__FILE__,__FUNCTION__,__LINE__)
/* #define valloc(s)      pam_valloc(s,__FILE__,__FUNCTION__,__LINE__) */
/* #define alloca(s)      pam_alloca(s,__FILE__,__FUNCTION__,__LINE__) */
#define exit(i)        pam_exit(i,__FILE__,__FUNCTION__,__LINE__)

/* these are the prototypes for the wrapper functions */

#include <sys/types.h>

extern void *pam_malloc(size_t s,const char *,const char *,const int);
extern void *pam_calloc(size_t n,size_t s,const char *,const char *,const int);
extern void  pam_free(void *x,const char *,const char *,const int);
extern void *pam_memalign(size_t a,size_t s
			 ,const char *,const char *,const int);
extern void *pam_realloc(void *x,size_t s,const char *,const char *,const int);
extern void *pam_valloc(size_t s,const char *,const char *,const int);
extern void *pam_alloca(size_t s,const char *,const char *,const int);
extern void  pam_exit(int i,const char *,const char *,const int);

/* these are the flags used to turn on and off diagnostics */

#define PAM_MALLOC_LEAKED             01
#define PAM_MALLOC_REQUEST            02
#define PAM_MALLOC_FREE               04
#define PAM_MALLOC_EXCH               (PAM_MALLOC_FREED|PAM_MALLOC_EXCH)
#define PAM_MALLOC_RESIZE            010
#define PAM_MALLOC_FAIL              020
#define PAM_MALLOC_NULL              040
#define PAM_MALLOC_VERIFY           0100
#define PAM_MALLOC_FUNC             0200
#define PAM_MALLOC_PAUSE            0400
#define PAM_MALLOC_STOP            01000

#define PAM_MALLOC_ALL              0777

#define PAM_MALLOC_DEFAULT     \
  (PAM_MALLOC_LEAKED|PAM_MALLOC_PAUSE|PAM_MALLOC_FAIL)

#include <stdio.h>

extern FILE *pam_malloc_outfile;      /* defaults to stdout */

/* how much output do you want? */

extern int pam_malloc_flags;
extern int pam_malloc_delay_length;      /* how long to pause on errors */

#endif /* PAM_MALLOC_H */
