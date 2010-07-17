/*
 * $FreeBSD: src/libexec/rtld-elf/libmap.h,v 1.4.22.1.4.1 2010/06/14 02:09:06 kensmith Exp $
 */

int	lm_init (char *);
void	lm_fini (void);
char *	lm_find (const char *, const char *);
#ifdef COMPAT_32BIT
char *	lm_findn (const char *, const char *, const int);
#endif
