/* This file is SYSV-ism (for compatibility only) */
/* this declarations must match stdlib.h ones */

#if !defined(_MALLOC_H_) && !defined(_STDLIB_H_)
#define _MALLOC_H_

#include <sys/cdefs.h>

__BEGIN_DECLS
void	*calloc __P((size_t, size_t));
void	 free __P((void *));
void	*malloc __P((size_t));
void	*realloc __P((void *, size_t));
__END_DECLS

#endif /* _MALLOC_H_ */
