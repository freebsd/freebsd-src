/* alloc.h */

typedef unsigned SIZE_T;

/* Like malloc and realloc, but don't return if no memory is available. */

extern UNIV xmalloc P((SIZE_T));
extern UNIV xrealloc P((UNIV, SIZE_T));
