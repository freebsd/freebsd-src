/*
 * Alpha IO and memory functions.. Just expand the inlines in the header
 * files..
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/string.h>

#include <asm/io.h>

u8 _inb(unsigned long addr)
{
	return __inb(addr);
}

u16 _inw(unsigned long addr)
{
	return __inw(addr);
}

u32 _inl(unsigned long addr)
{
	return __inl(addr);
}


void _outb(u8 b, unsigned long addr)
{
	__outb(b, addr);
}

void _outw(u16 b, unsigned long addr)
{
	__outw(b, addr);
}

void _outl(u32 b, unsigned long addr)
{
	__outl(b, addr);
}

u8 ___raw_readb(unsigned long addr)
{
	return __readb(addr);
}

u16 ___raw_readw(unsigned long addr)
{
	return __readw(addr);
}

u32 ___raw_readl(unsigned long addr)
{
	return __readl(addr);
}

u64 ___raw_readq(unsigned long addr)
{
	return __readq(addr);
}

u8 _readb(unsigned long addr)
{
	unsigned long r = __readb(addr);
	mb();
	return r;
}

u16 _readw(unsigned long addr)
{
	unsigned long r = __readw(addr);
	mb();
	return r;
}

u32 _readl(unsigned long addr)
{
	unsigned long r = __readl(addr);
	mb();
	return r;
}

u64 _readq(unsigned long addr)
{
	unsigned long r = __readq(addr);
	mb();
	return r;
}

void ___raw_writeb(u8 b, unsigned long addr)
{
	__writeb(b, addr);
}

void ___raw_writew(u16 b, unsigned long addr)
{
	__writew(b, addr);
}

void ___raw_writel(u32 b, unsigned long addr)
{
	__writel(b, addr);
}

void ___raw_writeq(u64 b, unsigned long addr)
{
	__writeq(b, addr);
}

void _writeb(u8 b, unsigned long addr)
{
	__writeb(b, addr);
	mb();
}

void _writew(u16 b, unsigned long addr)
{
	__writew(b, addr);
	mb();
}

void _writel(u32 b, unsigned long addr)
{
	__writel(b, addr);
	mb();
}

void _writeq(u64 b, unsigned long addr)
{
	__writeq(b, addr);
	mb();
}

/*
 * Read COUNT 8-bit bytes from port PORT into memory starting at
 * SRC.
 */
void insb (unsigned long port, void *dst, unsigned long count)
{
	while (((unsigned long)dst) & 0x3) {
		if (!count)
			return;
		count--;
		*(unsigned char *) dst = inb(port);
		((unsigned char *) dst)++;
	}

	while (count >= 4) {
		unsigned int w;
		count -= 4;
		w = inb(port);
		w |= inb(port) << 8;
		w |= inb(port) << 16;
		w |= inb(port) << 24;
		*(unsigned int *) dst = w;
		((unsigned int *) dst)++;
	}

	while (count) {
		--count;
		*(unsigned char *) dst = inb(port);
		((unsigned char *) dst)++;
	}
}


/*
 * Read COUNT 16-bit words from port PORT into memory starting at
 * SRC.  SRC must be at least short aligned.  This is used by the
 * IDE driver to read disk sectors.  Performance is important, but
 * the interfaces seems to be slow: just using the inlined version
 * of the inw() breaks things.
 */
void insw (unsigned long port, void *dst, unsigned long count)
{
	if (((unsigned long)dst) & 0x3) {
		if (((unsigned long)dst) & 0x1) {
			panic("insw: memory not short aligned");
		}
		if (!count)
			return;
		count--;
		*(unsigned short* ) dst = inw(port);
		((unsigned short *) dst)++;
	}

	while (count >= 2) {
		unsigned int w;
		count -= 2;
		w = inw(port);
		w |= inw(port) << 16;
		*(unsigned int *) dst = w;
		((unsigned int *) dst)++;
	}

	if (count) {
		*(unsigned short*) dst = inw(port);
	}
}


/*
 * Read COUNT 32-bit words from port PORT into memory starting at
 * SRC. Now works with any alignment in SRC. Performance is important,
 * but the interfaces seems to be slow: just using the inlined version
 * of the inl() breaks things.
 */
void insl (unsigned long port, void *dst, unsigned long count)
{
	unsigned int l = 0, l2;
	
	if (!count)
		return;
	
	switch (((unsigned long) dst) & 0x3)
	{
	 case 0x00:			/* Buffer 32-bit aligned */
		while (count--)
		{
			*(unsigned int *) dst = inl(port);
			((unsigned int *) dst)++;
		}
		break;
	
	/* Assuming little endian Alphas in cases 0x01 -- 0x03 ... */
	
	 case 0x02:			/* Buffer 16-bit aligned */
		--count;
		
		l = inl(port);
		*(unsigned short *) dst = l;
		((unsigned short *) dst)++;
		
		while (count--)
		{
			l2 = inl(port);
			*(unsigned int *) dst = l >> 16 | l2 << 16;
			((unsigned int *) dst)++;
			l = l2;
		}
		*(unsigned short *) dst = l >> 16;
		break;
	 case 0x01:			/* Buffer 8-bit aligned */
		--count;
		
		l = inl(port);
		*(unsigned char *) dst = l;
		((unsigned char *) dst)++;
		*(unsigned short *) dst = l >> 8;
		((unsigned short *) dst)++;
		while (count--)
		{
			l2 = inl(port);
			*(unsigned int *) dst = l >> 24 | l2 << 8;
			((unsigned int *) dst)++;
			l = l2;
		}
		*(unsigned char *) dst = l >> 24;
		break;
	 case 0x03:			/* Buffer 8-bit aligned */
		--count;
		
		l = inl(port);
		*(unsigned char *) dst = l;
		((unsigned char *) dst)++;
		while (count--)
		{
			l2 = inl(port);
			*(unsigned int *) dst = l << 24 | l2 >> 8;
			((unsigned int *) dst)++;
			l = l2;
		}
		*(unsigned short *) dst = l >> 8;
		((unsigned short *) dst)++;
		*(unsigned char *) dst = l >> 24;
		break;
	}
}


/*
 * Like insb but in the opposite direction.
 * Don't worry as much about doing aligned memory transfers:
 * doing byte reads the "slow" way isn't nearly as slow as
 * doing byte writes the slow way (no r-m-w cycle).
 */
void outsb(unsigned long port, const void * src, unsigned long count)
{
	while (count) {
		count--;
		outb(*(char *)src, port);
		((char *) src)++;
	}
}

/*
 * Like insw but in the opposite direction.  This is used by the IDE
 * driver to write disk sectors.  Performance is important, but the
 * interfaces seems to be slow: just using the inlined version of the
 * outw() breaks things.
 */
void outsw (unsigned long port, const void *src, unsigned long count)
{
	if (((unsigned long)src) & 0x3) {
		if (((unsigned long)src) & 0x1) {
			panic("outsw: memory not short aligned");
		}
		outw(*(unsigned short*)src, port);
		((unsigned short *) src)++;
		--count;
	}

	while (count >= 2) {
		unsigned int w;
		count -= 2;
		w = *(unsigned int *) src;
		((unsigned int *) src)++;
		outw(w >>  0, port);
		outw(w >> 16, port);
	}

	if (count) {
		outw(*(unsigned short *) src, port);
	}
}


/*
 * Like insl but in the opposite direction.  This is used by the IDE
 * driver to write disk sectors.  Works with any alignment in SRC.
 *  Performance is important, but the interfaces seems to be slow:
 * just using the inlined version of the outl() breaks things.
 */
void outsl (unsigned long port, const void *src, unsigned long count)
{
	unsigned int l = 0, l2;
	
	if (!count)
		return;
	
	switch (((unsigned long) src) & 0x3)
	{
	 case 0x00:			/* Buffer 32-bit aligned */
		while (count--)
		{
			outl(*(unsigned int *) src, port);
			((unsigned int *) src)++;
		}
		break;
	
	/* Assuming little endian Alphas in cases 0x01 -- 0x03 ... */
	
	 case 0x02:			/* Buffer 16-bit aligned */
		--count;
		
		l = *(unsigned short *) src << 16;
		((unsigned short *) src)++;
		
		while (count--)
		{
			l2 = *(unsigned int *) src;
			((unsigned int *) src)++;
			outl (l >> 16 | l2 << 16, port);
			l = l2;
		}
		l2 = *(unsigned short *) src;
		outl (l >> 16 | l2 << 16, port);
		break;
	 case 0x01:			/* Buffer 8-bit aligned */
		--count;
		
		l  = *(unsigned char *) src << 8;
		((unsigned char *) src)++;
		l |= *(unsigned short *) src << 16;
		((unsigned short *) src)++;
		while (count--)
		{
			l2 = *(unsigned int *) src;
			((unsigned int *) src)++;
			outl (l >> 8 | l2 << 24, port);
			l = l2;
		}
		l2 = *(unsigned char *) src;
		outl (l >> 8 | l2 << 24, port);
		break;
	 case 0x03:			/* Buffer 8-bit aligned */
		--count;
		
		l  = *(unsigned char *) src << 24;
		((unsigned char *) src)++;
		while (count--)
		{
			l2 = *(unsigned int *) src;
			((unsigned int *) src)++;
			outl (l >> 24 | l2 << 8, port);
			l = l2;
		}
		l2  = *(unsigned short *) src;
		((unsigned short *) src)++;
		l2 |= *(unsigned char *) src << 16;
		outl (l >> 24 | l2 << 8, port);
		break;
	}
}


/*
 * Copy data from IO memory space to "real" memory space.
 * This needs to be optimized.
 */
void _memcpy_fromio(void * to, unsigned long from, long count)
{
	/* Optimize co-aligned transfers.  Everything else gets handled
	   a byte at a time. */

	if (count >= 8 && ((long)to & 7) == (from & 7)) {
		count -= 8;
		do {
			*(u64 *)to = __raw_readq(from);
			count -= 8;
			to += 8;
			from += 8;
		} while (count >= 0);
		count += 8;
	}

	if (count >= 4 && ((long)to & 3) == (from & 3)) {
		count -= 4;
		do {
			*(u32 *)to = __raw_readl(from);
			count -= 4;
			to += 4;
			from += 4;
		} while (count >= 0);
		count += 4;
	}
		
	if (count >= 2 && ((long)to & 1) == (from & 1)) {
		count -= 2;
		do {
			*(u16 *)to = __raw_readw(from);
			count -= 2;
			to += 2;
			from += 2;
		} while (count >= 0);
		count += 2;
	}

	while (count > 0) {
		*(u8 *) to = __raw_readb(from);
		count--;
		to++;
		from++;
	}
}

/*
 * Copy data from "real" memory space to IO memory space.
 * This needs to be optimized.
 */
void _memcpy_toio(unsigned long to, const void * from, long count)
{
	/* Optimize co-aligned transfers.  Everything else gets handled
	   a byte at a time. */
	/* FIXME -- align FROM.  */

	if (count >= 8 && (to & 7) == ((long)from & 7)) {
		count -= 8;
		do {
			__raw_writeq(*(const u64 *)from, to);
			count -= 8;
			to += 8;
			from += 8;
		} while (count >= 0);
		count += 8;
	}

	if (count >= 4 && (to & 3) == ((long)from & 3)) {
		count -= 4;
		do {
			__raw_writel(*(const u32 *)from, to);
			count -= 4;
			to += 4;
			from += 4;
		} while (count >= 0);
		count += 4;
	}
		
	if (count >= 2 && (to & 1) == ((long)from & 1)) {
		count -= 2;
		do {
			__raw_writew(*(const u16 *)from, to);
			count -= 2;
			to += 2;
			from += 2;
		} while (count >= 0);
		count += 2;
	}

	while (count > 0) {
		__raw_writeb(*(const u8 *) from, to);
		count--;
		to++;
		from++;
	}
	mb();
}

/*
 * "memset" on IO memory space.
 */
void _memset_c_io(unsigned long to, unsigned long c, long count)
{
	/* Handle any initial odd byte */
	if (count > 0 && (to & 1)) {
		__raw_writeb(c, to);
		to++;
		count--;
	}

	/* Handle any initial odd halfword */
	if (count >= 2 && (to & 2)) {
		__raw_writew(c, to);
		to += 2;
		count -= 2;
	}

	/* Handle any initial odd word */
	if (count >= 4 && (to & 4)) {
		__raw_writel(c, to);
		to += 4;
		count -= 4;
	}

	/* Handle all full-sized quadwords: we're aligned
	   (or have a small count) */
	count -= 8;
	if (count >= 0) {
		do {
			__raw_writeq(c, to);
			to += 8;
			count -= 8;
		} while (count >= 0);
	}
	count += 8;

	/* The tail is word-aligned if we still have count >= 4 */
	if (count >= 4) {
		__raw_writel(c, to);
		to += 4;
		count -= 4;
	}

	/* The tail is half-word aligned if we have count >= 2 */
	if (count >= 2) {
		__raw_writew(c, to);
		to += 2;
		count -= 2;
	}

	/* And finally, one last byte.. */
	if (count) {
		__raw_writeb(c, to);
	}
	mb();
}

void
scr_memcpyw(u16 *d, const u16 *s, unsigned int count)
{
	if (! __is_ioaddr((unsigned long) s)) {
		/* Source is memory.  */
		if (! __is_ioaddr((unsigned long) d))
			memcpy(d, s, count);
		else
			memcpy_toio(d, s, count);
	} else {
		/* Source is screen.  */
		if (! __is_ioaddr((unsigned long) d))
			memcpy_fromio(d, s, count);
		else {
			/* FIXME: Should handle unaligned ops and
			   operation widening.  */
			count /= 2;
			while (count--) {
				u16 tmp = __raw_readw((unsigned long)(s++));
				__raw_writew(tmp, (unsigned long)(d++));
			}
		}
	}
}
