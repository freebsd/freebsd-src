/*
 * $FreeBSD: src/libexec/rtld-elf/libmap.h,v 1.3.2.1 2005/02/27 20:47:20 mdodd Exp $
 */

int	lm_init (char *);
void	lm_fini (void);
char *	lm_find (const char *, const char *);
#ifdef COMPAT_32BIT
char *	lm_findn (const char *, const char *, const int);
#endif
