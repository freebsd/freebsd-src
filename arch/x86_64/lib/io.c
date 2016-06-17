#include <linux/string.h>
#include <linux/module.h>
#include <asm/io.h>

void *memcpy_toio(void *dst,const void*src,unsigned len)
{
	return __inline_memcpy(__io_virt(dst),src,len);
}

void *memcpy_fromio(void *dst,const void*src,unsigned len)
{
	return __inline_memcpy(dst,__io_virt(src),len);
}
