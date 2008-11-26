#include <machine/asm.h>
__FBSDID("$FreeBSD: src/lib/msun/amd64/s_llrintf.S,v 1.1.16.1 2008/10/02 02:57:24 kensmith Exp $")
	
/* sizeof(long) == sizeof(long long) */
#define	fn	llrintf
#include "s_lrintf.S"
