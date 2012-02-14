/* $FreeBSD$ */

#include "itbl-mips.h"

/* Choose a default ABI for MIPS targets.  */
/* XXX: Where should this be ? */
#define MIPS_DEFAULT_ABI NO_ABI

/* Default CPU for MIPS targets.  */
#define MIPS_CPU_STRING_DEFAULT "from-abi"

/* Generate 64-bit code by default on MIPS targets.  */
#define MIPS_DEFAULT_64BIT 0

/* Allow use of E_MIPS_ABI_O32 on MIPS targets.  */
#define USE_E_MIPS_ABI_O32 1

/* Use traditional mips */
#define TE_TMIPS 1
