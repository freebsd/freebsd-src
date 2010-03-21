/*
 * $FreeBSD: src/libexec/rtld-elf/libmap.h,v 1.4.24.1 2010/02/10 00:26:20 kensmith Exp $
 */

int	lm_init (char *);
void	lm_fini (void);
char *	lm_find (const char *, const char *);
#ifdef COMPAT_32BIT
char *	lm_findn (const char *, const char *, const int);
#endif
