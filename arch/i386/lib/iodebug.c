#include <asm/io.h>

void * __io_virt_debug(unsigned long x, const char *file, int line)
{
	if (x < PAGE_OFFSET) {
		printk("io mapaddr 0x%05lx not valid at %s:%d!\n", x, file, line);
		return __va(x);
	}
	return (void *)x;
}

unsigned long __io_phys_debug(unsigned long x, const char *file, int line)
{
	if (x < PAGE_OFFSET) {
		printk("io mapaddr 0x%05lx not valid at %s:%d!\n", x, file, line);
		return x;
	}
	return __pa(x);
}
