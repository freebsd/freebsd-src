/* Machine specific defines for the dpx2 machine.  */

/* The magic number is not the usual MC68MAGIC.  */
#define COFF_MAGIC       MC68KBCSMAGIC

#define REGISTER_PREFIX_OPTIONAL 1

#define TARGET_FORMAT "coff-m68k-un"

#include "obj-format.h"

/* end of te-dpx2.h */
