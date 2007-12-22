@BOTTOM@

#ifdef BROKEN_REALLOC
#define realloc(X, Y) isoc_realloc((X), (Y))
#define isoc_realloc(X, Y) ((X) ? realloc((X), (Y)) : malloc(Y))
#endif

#ifdef VOID_RETSIGTYPE
#define SIGRETURN(x) return
#else
#define SIGRETURN(x) return (RETSIGTYPE)(x)
#endif

#define RCSID(msg) \
static /**/const char *const rcsid[] = { (const char *)rcsid, "\100(#)" msg }

#undef PROTOTYPES

/* Maximum values on all known systems */
#define MaxHostNameLen (64+4)
#define MaxPathLen (1024+4)

/*
 * Define NDBM if you are using the 4.3 ndbm library (which is part of
 * libc).  If not defined, 4.2 dbm will be assumed.
 */
#if defined(HAVE_DBM_FIRSTKEY)
#define NDBM
#endif

/*
 * Defining this enables lots of useful (and used) extensions on
 * glibc-based systems such as Linux
 */

#define _GNU_SOURCE
