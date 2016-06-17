#include <linux/module.h>
#include <linux/types.h>

#include <asm/io.h>

/*
 * Copy data from IO memory space to "real" memory space.
 * This needs to be optimized.
 */
void _memcpy_fromio(void * to, unsigned long from, size_t count)
{
	while (count) {
		count--;
		*(char *) to = readb(from);
		((char *) to)++;
		from++;
	}
}

/*
 * Copy data from "real" memory space to IO memory space.
 * This needs to be optimized.
 */
void _memcpy_toio(unsigned long to, const void * from, size_t count)
{
	while (count) {
		count--;
		writeb(*(char *) from, to);
		((char *) from)++;
		to++;
	}
}

/*
 * "memset" on IO memory space.
 * This needs to be optimized.
 */
void _memset_io(unsigned long dst, int c, size_t count)
{
	while (count) {
		count--;
		writeb(c, dst);
		dst++;
	}
}

EXPORT_SYMBOL(_memcpy_fromio);
EXPORT_SYMBOL(_memcpy_toio);
EXPORT_SYMBOL(_memset_io);
