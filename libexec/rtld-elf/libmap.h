/*
 * $FreeBSD: src/libexec/rtld-elf/libmap.h,v 1.3 2004/03/21 01:21:26 peter Exp $
 */

int	lm_init (void);
void	lm_fini (void);
char *	lm_find (const char *, const char *);
#ifdef COMPAT_32BIT
char *	lm_findn (const char *, const char *, const int);
#endif
