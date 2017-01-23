#include "SYS.h"
/*
 * XXXAR: We can't just use RSYSCALL as that would try to perform link-time
 * relocations against __cerror and this cannot work before libc is relocated.
 * As this is only used for rtld_printf() which doesn't check errno,
 * we can just skip setting errno.
 */
LEAF(__rtld_write)
	.hidden _C_LABEL(__rtld_write)
	PIC_PROLOGUE(__sys_ ## x);
	SYSTRAP(write);
	PIC_RETURN()
END(__rtld_write)
