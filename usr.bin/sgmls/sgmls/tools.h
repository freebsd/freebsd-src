/* TOOLS.H: Definitions for type declarations, printing, bit handling, etc.
*/

#if CHAR_SIGNED
typedef unsigned char UNCH;
#else
typedef char UNCH;
#endif

#if CHAR_SIGNED
#define ustrcmp(s1, s2) strcmp((char *)(s1), (char *)(s2))
#define ustrcpy(s1, s2) strcpy((char *)(s1), (char *)(s2))
#define ustrchr(s, c) (UNCH *)strchr((char *)(s), c)
#define ustrncmp(s1, s2, n) strncmp((char *)(s1), (char *)(s2), n)
#define ustrncpy(s1, s2, n) strncpy((char *)(s1), (char *)(s2), n)
#define ustrlen(s1) strlen((char *)(s1))
#else
#define ustrcmp strcmp
#define ustrcpy strcpy
#define ustrchr strchr
#define ustrncmp strncmp
#define ustrncpy strncpy
#define ustrlen strlen
#endif

#if 0
int ustrcmp(UNCH *, UNCH *);
UNCH *ustrchr(UNCH *, int);
int ustrncmp(UNCH *, UNCH *, UNS);
int ustrncpy(UNCH *, UNCH *, UNS);
int ustrlen(UNCH *);
#endif

typedef unsigned UNS;

#ifdef USE_ISASCII
#define ISASCII(c) isascii(c)
#else
#define ISASCII(c) (1)
#endif

#ifdef BSD_STRINGS
#define MEMZERO(s, n) bzero(s, n)
#else /* not BSD_STRINGS */
#define MEMZERO(s, n) memset(s, '\0', n)
#endif /* not BSD_STRINGS */

/* Macros for bit manipulation.
*/
#define SET(word, bits)          ((word) |= (bits))    /* Turn bits on */
#define RESET(word, bits)        ((word) &= ~(bits))   /* Turn bits off */
#define GET(word, bits)          ((word) & (bits))     /* 1=any bit on */
#define BITOFF(word, bits)       (GET(word, bits)==0)  /* 1=no bits on */
#define BITON(word, bits)        ((word) & (bits))     /* 1=any bit on */

#define ETDCDATA (dumetd)          /* Dummy etd pointer for #PCDATA. */
#define ETDNULL  (dumetd + 1)      /* Dummy etd pointer for null tag. */
#define ETDNET   (dumetd + 2)      /* Dummy etd pointer for NET delimiter. */
#define BADPTR(p) \
  ((p) == NULL || (p) == ETDCDATA || (p) == ETDNULL || (p) == ETDNET)
#define PTRNUM(p) ((p) == NULL ? 0 : ((p) - dumetd) + 1)

#ifdef USE_PROTOTYPES
#define P(parms) parms
#else
#define P(parms) ()
#endif

/* VP is used for prototypes of varargs functions.  You can't have a
prototype if the function is defined using varargs.h rather than
stdarg.h. */
#ifdef VARARGS
#define VP(parms) ()
#else
#define VP(parms) P(parms)
#endif
