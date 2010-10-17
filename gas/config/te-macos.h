/* This file is te-macos.h.  */

#define TE_POWERMAC 1

/* Added these, because if we don't know what we're targeting we may
   need an assembler version of libgcc, and that will use local
   labels.  */
#define LOCAL_LABELS_DOLLAR 1
#define LOCAL_LABELS_FB 1

#include "obj-format.h"
