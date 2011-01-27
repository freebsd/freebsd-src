#include <machine/asm.h>
__FBSDID("$FreeBSD$");

#define MEMMOVE
#include "bcopy.S"

	.section .note.GNU-stack,"",%progbits
