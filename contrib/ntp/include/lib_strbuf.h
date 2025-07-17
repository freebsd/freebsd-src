/*
 * lib_strbuf.h - definitions for routines which use the common string buffers
 */
#ifndef LIB_STRBUF_H
#define LIB_STRBUF_H

#include <ntp_types.h>
#include <ntp_malloc.h>			/* for zero_mem() */

#define	LIB_BUFLENGTH	128

extern int	lib_inited;
extern int	ipv4_works;
extern int	ipv6_works;

extern	void	init_lib(void);

/*
 * Get a pointer to the next string buffer of LIB_BUFLENGTH octets.
 * New and modified code should use buf = lib_getbuf() directly to
 * provide clarity for folks familiar with common C style, but there's
 * no need to churn the history with a mechanical switch away from
 * LIB_GETBUF(buf).
 */
extern	char* lib_getbuf(void);

#define	LIB_GETBUF(bufp)		\
	do {				\
		(bufp) = lib_getbuf();	\
	} while (FALSE)

#endif	/* LIB_STRBUF_H */
