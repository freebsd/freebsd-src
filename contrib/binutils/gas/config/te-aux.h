#define TE_AUX

/* From obj-coff.h:
   This internal_lineno crap is to stop namespace pollution from the
   bfd internal coff headerfile. */
#define internal_lineno bfd_internal_lineno
#include "coff/aux-coff.h"	/* override bits in coff/internal.h */
#undef internal_lineno

#define COFF_NOLOAD_PROBLEM
#define KEEP_RELOC_INFO

#include "obj-format.h"

#ifndef LOCAL_LABELS_FB
#define LOCAL_LABELS_FB 1
#endif
