/*
 * Provide buffer space for various items.
 *
 * We do this in the text segment so that we can control the location
 * of the buffers at the link stage, and thus ensure that they are
 * within reach of the real-mode support routines.
 *
 * It is especially important to have the stack down low so that
 * it is reachable from both 32-bit and 16-bit code.
 */

#include <machine/asm.h>	

	.text
	.align 4
ENTRY(diskbuf)
	. = CNAME(diskbuf) + (DISKBUFSIZE * 512)
ENTRY(diskbuf_user)
	.long	0
ENTRY(diskbuf_size)
	.long DISKBUFSIZE
	
	.align 4
ENTRY(stackbase)
	. = CNAME(stackbase) + STACKSIZE
ENTRY(stacktop)
	.long	0
