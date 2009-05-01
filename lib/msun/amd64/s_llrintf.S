#include <machine/asm.h>
__FBSDID("$FreeBSD: src/lib/msun/amd64/s_llrintf.S,v 1.1.20.1 2009/04/15 03:14:26 kensmith Exp $")
	
/* sizeof(long) == sizeof(long long) */
#define	fn	llrintf
#include "s_lrintf.S"
