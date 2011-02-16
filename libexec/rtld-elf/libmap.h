/*
 * $FreeBSD: src/libexec/rtld-elf/libmap.h,v 1.4.26.1 2010/12/21 17:10:29 kensmith Exp $
 */

int	lm_init (char *);
void	lm_fini (void);
char *	lm_find (const char *, const char *);
#ifdef COMPAT_32BIT
char *	lm_findn (const char *, const char *, const int);
#endif
