#ifndef _TERMCAP_H
#define _TERMCAP_H	1

#ifdef __FreeBSD__
#include <sys/cdefs.h>
#else
#ifndef __P
#if defined(__STDC__) || defined(__cplusplus)
#define __P(protos) protos
#else
#define	__P(protos)	()		/* traditional C preprocessor */
#endif
#endif
#ifndef __BEGIN_DECLS
#ifdef __cplusplus
#define __BEGIN_DECLS extern "C" {
#define __END_DECLS   };
#else
#define __BEGIN_DECLS
#define __END_DECLS
#endif
#endif
#endif

__BEGIN_DECLS

#ifndef __FreeBSD__
#include <sys/types.h>
#endif

extern char PC;
extern char *UP;
extern char *BC;
#ifdef __FreeBSD__
extern short ospeed;
#else
extern speed_t ospeed;
#endif

extern int tgetent __P((char *, const char *));
extern int tgetflag __P((const char *));
extern int tgetnum __P((const char *));
extern char *tgetstr __P((const char *, char **));

extern int tputs __P((const char *, int, int (*)(int)));

extern char *tgoto __P((const char *, int, int));
extern char *tparam __P((const char *, char *, int, ...));

__END_DECLS

#endif /* _TERMCAP_H */
