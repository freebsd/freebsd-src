#define	__strrchr_aarch64	strrchr
#include "aarch64/strrchr.S"

.weak rindex
.equ rindex, strrchr
