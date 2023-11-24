#define	__memcpy_aarch64	memcpy
#define	__memmove_aarch64	memmove
#include "aarch64/memcpy.S"

.weak index
.equ index, strchr
