/* This file is te-delta88.h.  */

#define TE_DELTA88 1

#define COFF_NOLOAD_PROBLEM	1

/* Added these, because if we don't know what we're targeting we may
   need an assembler version of libgcc, and that will use local
   labels.  */
#define LOCAL_LABELS_DOLLAR	1
#define LOCAL_LABELS_FB		1

#include "obj-format.h"
