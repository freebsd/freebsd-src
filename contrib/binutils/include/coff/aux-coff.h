/* Modifications of internal.h and m68k.h needed by A/UX
   Suggested by Ian Lance Taylor <ian@cygnus.com> */

#ifndef GNU_COFF_AUX_H
#define GNU_COFF_AUX_H 1

#include "coff/internal.h"
#include "coff/m68k.h"

/* Section contains 64-byte padded pathnames of shared libraries */
#undef STYP_LIB
#define STYP_LIB 0x200

/* Section contains shared library initialization code */
#undef STYP_INIT
#define STYP_INIT 0x400

/* Section contains .ident information */
#undef STYP_IDENT
#define STYP_IDENT 0x800

/* Section types used by bfd and gas not defined (directly) by A/UX */
#undef STYP_OVER
#define STYP_OVER 0
#undef STYP_INFO
#define STYP_INFO STYP_IDENT

/* Traditional name of the section tagged with STYP_LIB */
#define _LIB ".lib"

#endif /* GNU_COFF_AUX_H */
