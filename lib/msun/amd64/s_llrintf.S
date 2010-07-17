#include <machine/asm.h>
__FBSDID("$FreeBSD: src/lib/msun/amd64/s_llrintf.S,v 1.1.22.1.4.1 2010/06/14 02:09:06 kensmith Exp $")
	
/* sizeof(long) == sizeof(long long) */
#define	fn	llrintf
#include "s_lrintf.S"
