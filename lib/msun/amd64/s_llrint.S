#include <machine/asm.h>
__FBSDID("$FreeBSD: src/lib/msun/amd64/s_llrint.S,v 1.1.20.1 2008/11/25 02:59:29 kensmith Exp $")
	
/* sizeof(long) == sizeof(long long) */
#define	fn	llrint
#include "s_lrint.S"
