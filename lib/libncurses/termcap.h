

#ifndef _TERMCAP_H
#define _TERMCAP_H	1

#ifdef __cplusplus
extern "C" 
{
#endif /* __cplusplus */

#include <sys/cdefs.h>

extern char PC;
extern char *UP;
extern char *BC;
extern short ospeed;

extern int tgetent __P((char *, const char *));
extern int tgetflag __P((const char *));
extern int tgetnum __P((const char *));
extern char *tgetstr __P((const char *, char **));

extern int tputs __P((const char *, int, int (*)(int)));

extern char *tgoto __P((const char *, int, int));
extern char *tparam __P((const char *, char *, int, ...));

#ifdef __cplusplus
}
#endif

#endif /* _TERMCAP_H */
