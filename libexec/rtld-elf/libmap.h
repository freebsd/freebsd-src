/*
 * $FreeBSD: src/libexec/rtld-elf/libmap.h,v 1.1 2003/04/07 16:21:25 mdodd Exp $
 */

void	lm_init (void);
void	lm_fini (void);
char *	lm_find (const char *, const char *);
