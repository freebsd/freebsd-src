/*
 * I/O routine for SH-2000
 */
#include <linux/config.h>
#include <asm/io.h>
#include <asm/machvec.h>

#define IDE_OFFSET    0xb6200000
#define NIC_OFFSET    0xb6000000
#define EXTBUS_OFFSET 0xba000000

unsigned long sh2000_isa_port2addr(unsigned long offset)
{
	if((offset & ~7) == 0x1f0 || offset == 0x3f6)
		return IDE_OFFSET + offset;
	else if((offset & ~0x1f) == 0x300)
		return NIC_OFFSET + offset;
	return EXTBUS_OFFSET + offset;
}
