/*
 * $FreeBSD: src/libexec/rtld-elf/libmap.h,v 1.4.18.1 2008/11/25 02:59:29 kensmith Exp $
 */

int	lm_init (char *);
void	lm_fini (void);
char *	lm_find (const char *, const char *);
#ifdef COMPAT_32BIT
char *	lm_findn (const char *, const char *, const int);
#endif
