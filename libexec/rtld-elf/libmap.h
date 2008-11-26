/*
 * $FreeBSD: src/libexec/rtld-elf/libmap.h,v 1.4.16.1 2008/10/02 02:57:24 kensmith Exp $
 */

int	lm_init (char *);
void	lm_fini (void);
char *	lm_find (const char *, const char *);
#ifdef COMPAT_32BIT
char *	lm_findn (const char *, const char *, const int);
#endif
