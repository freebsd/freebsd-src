
/* Whether malloc must be declared even if <stdlib.h> is included.  */
#undef NEED_DECLARATION_MALLOC

/* Whether free must be declared even if <stdlib.h> is included.  */
#undef NEED_DECLARATION_FREE
@TOP@

/* Do we need to use the b modifier when opening binary files?  */
#undef USE_BINARY_FOPEN

/* Name of host specific header file to include in trad-core.c.  */
#undef TRAD_HEADER

/* Define only if <sys/procfs.h> is available *and* it defines prstatus_t.  */
#undef HAVE_SYS_PROCFS_H

/* Do we really want to use mmap if it's available?  */
#undef USE_MMAP
