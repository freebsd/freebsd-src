/*
 * $FreeBSD$
 */

#ifndef LIBMAP_H
#define	LIBMAP_H

int	lm_init(const char *);
void	lm_fini(void);
char	*lm_find(const char *, const char *);
char	*lm_findn(const char *, const char *, const size_t);

#endif
