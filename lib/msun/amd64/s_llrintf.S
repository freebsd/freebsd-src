#include <machine/asm.h>
/* sizeof(long) == sizeof(long long) */
#define	fn	llrintf
#include "s_lrintf.S"
