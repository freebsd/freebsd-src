/*
 * $FreeBSD: src/libexec/rtld-elf/libmap.h,v 1.4.20.1 2009/04/15 03:14:26 kensmith Exp $
 */

int	lm_init (char *);
void	lm_fini (void);
char *	lm_find (const char *, const char *);
#ifdef COMPAT_32BIT
char *	lm_findn (const char *, const char *, const int);
#endif
