/*
 * $FreeBSD: src/libexec/rtld-elf/libmap.h,v 1.2 2003/09/13 21:43:08 mdodd Exp $
 */

int	lm_init (void);
void	lm_fini (void);
char *	lm_find (const char *, const char *);
