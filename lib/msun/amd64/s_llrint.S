#include <machine/asm.h>
/* sizeof(long) == sizeof(long long) */
#define	fn	llrint
#include "s_lrint.S"
