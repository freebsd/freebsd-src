#define	__strchr_aarch64	strchr
#include "aarch64/strchr.S"

.weak index
.equ index, strchr
