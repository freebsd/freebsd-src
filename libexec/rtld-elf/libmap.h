/*
 * $FreeBSD: src/libexec/rtld-elf/libmap.h,v 1.2.4.1 2004/02/03 21:04:16 fjoe Exp $
 */

int	lm_init (void);
void	lm_fini (void);
char *	lm_find (const char *, const char *);
