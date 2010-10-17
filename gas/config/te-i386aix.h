/* This file is te-i386aix.h and is built from pieces of code from
   Minh Tran-Le <TRANLE@INTELLICORP.COM> by rich@cygnus.com.  */

#define TE_I386AIX 1

#include "obj-format.h"

/* Undefine REVERSE_SORT_RELOCS to keep the relocation entries sorted
   in ascending vaddr.  */
#undef REVERSE_SORT_RELOCS

/* Define KEEP_RELOC_INFO so that the strip reloc info flag F_RELFLG is
   not used in the filehdr for COFF output.  */
#define KEEP_RELOC_INFO

/*
 * Local Variables:
 * comment-column: 0
 * fill-column: 79
 * End:
 */

/* end of te-i386aix.h */
