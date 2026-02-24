/* Used when we know we have a 64-byte dc zva instruction */
#define	__memset_aarch64	__memset_aarch64_zva64
#define	SKIP_ZVA_CHECK
#include "aarch64/memset.S"
