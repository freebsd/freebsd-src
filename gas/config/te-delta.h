#define TE_DELTA

#include "obj-format.h"

#define COFF_NOLOAD_PROBLEM	1
#define COFF_COMMON_ADDEND	1

/* Added these, because if we don't know what we're targeting we may
   need an assembler version of libgcc, and that will use local
   labels.  */
#define LOCAL_LABELS_DOLLAR	1
#define LOCAL_LABELS_FB		1

/* end of te-delta.h */
