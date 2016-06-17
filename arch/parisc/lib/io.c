/*
 * arch/parisc/lib/io.c
 *
 * Copyright (c) Matthew Wilcox 2001 for Hewlett-Packard
 * Copyright (c) Randolph Chung 2001 <tausq@debian.org>
 *
 * IO accessing functions which shouldn't be inlined because they're too big
 */

#include <linux/kernel.h>
#include <asm/io.h>

/* Copies a block of memory to a device in an efficient manner.
 * Assumes the device can cope with 32-bit transfers.  If it can't,
 * don't use this function.
 */
void memcpy_toio(unsigned long dest, const void *src, int count)
{
	if ((dest & 3) != ((unsigned long)src & 3))
		goto bytecopy;
	while (dest & 3) {
		writeb(*(char *)src, dest++);
		((char *)src)++;
		count--;
	}
	while (count > 3) {
		__raw_writel(*(u32 *)src, dest);
		(unsigned long) src += 4;
		dest += 4;
		count -= 4;
	}
 bytecopy:
	while (count--) {
		writeb(*(char *)src, dest++);
		((char *)src)++;
	}
}

/*
** Copies a block of memory from a device in an efficient manner.
** Assumes the device can cope with 32-bit transfers.  If it can't,
** don't use this function.
**
** CR16 counts on C3000 reading 256 bytes from Symbios 896 RAM:
**	27341/64    = 427 cyc per int
**	61311/128   = 478 cyc per short
**	122637/256  = 479 cyc per byte
** Ergo bus latencies dominant (not transfer size).
**      Minimize total number of transfers at cost of CPU cycles.
**	TODO: only look at src alignment and adjust the stores to dest.
*/
void memcpy_fromio(void *dest, unsigned long src, int count)
{
	/* first compare alignment of src/dst */ 
	if ( (((unsigned long)dest ^ src) & 1) || (count < 2) )
		goto bytecopy;

	if ( (((unsigned long)dest ^ src) & 2) || (count < 4) )
		goto shortcopy;

	/* Then check for misaligned start address */
	if (src & 1) {
		*(u8 *)dest = readb(src);
		((u8 *)src)++;
		((u8 *)dest)++;
		count--;
		if (count < 2) goto bytecopy;
	}

	if (src & 2) {
		*(u16 *)dest = __raw_readw(src);
		((u16 *)src)++;
		((u16 *)dest)++;
		count-=2;
	}

	while (count > 3) {
		*(u32 *)dest = __raw_readl(src);
		dest += 4;
		src += 4;
		count -= 4;
	}

 shortcopy:
	while (count > 1) {
		*(u16 *)dest = __raw_readw(src);
		((u16 *)src)++;
		((u16 *)dest)++;
		count-=2;
	}

 bytecopy:
	while (count--) {
		*(char *)dest = readb(src);
		((char *)src)++;
		((char *)dest)++;
	}
}

/* Sets a block of memory on a device to a given value.
 * Assumes the device can cope with 32-bit transfers.  If it can't,
 * don't use this function.
 */
void memset_io(unsigned long dest, char fill, int count)
{
	u32 fill32 = (fill << 24) | (fill << 16) | (fill << 8) | fill;
	while (dest & 3) {
		writeb(fill, dest++);
		count--;
	}
	while (count > 3) {
		__raw_writel(fill32, dest);
		dest += 4;
		count -= 4;
	}
	while (count--) {
		writeb(fill, dest++);
	}
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
		w = inb(port) << 24;
		w |= inb(port) << 16;
		w |= inb(port) << 8;
		w |= inb(port);
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
	unsigned int l = 0, l2;
	
	if (!count)
		return;
	
	switch (((unsigned long) dst) & 0x3)
	{
	 case 0x00:			/* Buffer 32-bit aligned */
		while (count>=2) {
			
			count -= 2;
			l = cpu_to_le16(inw(port)) << 16;
			l |= cpu_to_le16(inw(port));
			*(unsigned int *) dst = l;
			((unsigned int *) dst)++;
		}
		if (count) {
			*(unsigned short *) dst = cpu_to_le16(inw(port));
		}
		break;
	
	 case 0x02:			/* Buffer 16-bit aligned */
		*(unsigned short *) dst = cpu_to_le16(inw(port));
		((unsigned short *) dst)++;
		count--;
		while (count>=2) {
			
			count -= 2;
			l = cpu_to_le16(inw(port)) << 16;
			l |= cpu_to_le16(inw(port));
			*(unsigned int *) dst = l;
			((unsigned int *) dst)++;
		}
		if (count) {
			*(unsigned short *) dst = cpu_to_le16(inw(port));
		}
		break;
		
	 case 0x01:			/* Buffer 8-bit aligned */
	 case 0x03:
		/* I don't bother with 32bit transfers
		 * in this case, 16bit will have to do -- DE */
		--count;
		
		l = cpu_to_le16(inw(port));
		*(unsigned char *) dst = l >> 8;
		((unsigned char *) dst)++;
		while (count--)
		{
			l2 = cpu_to_le16(inw(port));
			*(unsigned short *) dst = (l & 0xff) << 8 | (l2 >> 8);
			((unsigned short *) dst)++;
			l = l2;
		}
		*(unsigned char *) dst = l & 0xff;
		break;
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
			*(unsigned int *) dst = cpu_to_le32(inl(port));
			((unsigned int *) dst)++;
		}
		break;
	
	 case 0x02:			/* Buffer 16-bit aligned */
		--count;
		
		l = cpu_to_le32(inl(port));
		*(unsigned short *) dst = l >> 16;
		((unsigned short *) dst)++;
		
		while (count--)
		{
			l2 = cpu_to_le32(inl(port));
			*(unsigned int *) dst = (l & 0xffff) << 16 | (l2 >> 16);
			((unsigned int *) dst)++;
			l = l2;
		}
		*(unsigned short *) dst = l & 0xffff;
		break;
	 case 0x01:			/* Buffer 8-bit aligned */
		--count;
		
		l = cpu_to_le32(inl(port));
		*(unsigned char *) dst = l >> 24;
		((unsigned char *) dst)++;
		*(unsigned short *) dst = (l >> 8) & 0xffff;
		((unsigned short *) dst)++;
		while (count--)
		{
			l2 = cpu_to_le32(inl(port));
			*(unsigned int *) dst = (l & 0xff) << 24 | (l2 >> 8);
			((unsigned int *) dst)++;
			l = l2;
		}
		*(unsigned char *) dst = l & 0xff;
		break;
	 case 0x03:			/* Buffer 8-bit aligned */
		--count;
		
		l = cpu_to_le32(inl(port));
		*(unsigned char *) dst = l >> 24;
		((unsigned char *) dst)++;
		while (count--)
		{
			l2 = cpu_to_le32(inl(port));
			*(unsigned int *) dst = (l & 0xffffff) << 8 | l2 >> 24;
			((unsigned int *) dst)++;
			l = l2;
		}
		*(unsigned short *) dst = (l >> 8) & 0xffff;
		((unsigned short *) dst)++;
		*(unsigned char *) dst = l & 0xff;
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
	unsigned int l = 0, l2;
	
	if (!count)
		return;
	
	switch (((unsigned long) src) & 0x3)
	{
	 case 0x00:			/* Buffer 32-bit aligned */
		while (count>=2) {
			count -= 2;
			l = *(unsigned int *) src;
			((unsigned int *) src)++;
			outw(le16_to_cpu(l >> 16), port);
			outw(le16_to_cpu(l & 0xffff), port);
		}
		if (count) {
			outw(le16_to_cpu(*(unsigned short*)src), port);
		}
		break;
	
	 case 0x02:			/* Buffer 16-bit aligned */
		
		outw(le16_to_cpu(*(unsigned short*)src), port);
		((unsigned short *) src)++;
		count--;
		
		while (count>=2) {
			count -= 2;
			l = *(unsigned int *) src;
			((unsigned int *) src)++;
			outw(le16_to_cpu(l >> 16), port);
			outw(le16_to_cpu(l & 0xffff), port);
		}
		if (count) {
			outw(le16_to_cpu(*(unsigned short*)src), port);
		}
		break;
		
	 case 0x01:			/* Buffer 8-bit aligned */	
		/* I don't bother with 32bit transfers
		 * in this case, 16bit will have to do -- DE */
		
		l  = *(unsigned char *) src << 8;
		((unsigned char *) src)++;
		count--;
		while (count)
		{
			count--;
			l2 = *(unsigned short *) src;
			((unsigned short *) src)++;
			outw(le16_to_cpu(l | l2 >> 8), port);
		        l = l2 << 8;
		}
		l2 = *(unsigned char *) src;
		outw (le16_to_cpu(l | l2>>8), port);
		break;
	
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
			outl(le32_to_cpu(*(unsigned int *) src), port);
			((unsigned int *) src)++;
		}
		break;
	
	 case 0x02:			/* Buffer 16-bit aligned */
		--count;
		
		l = *(unsigned short *) src;
		((unsigned short *) src)++;
		
		while (count--)
		{
			l2 = *(unsigned int *) src;
			((unsigned int *) src)++;
			outl (le32_to_cpu(l << 16 | l2 >> 16), port);
			l = l2;
		}
		l2 = *(unsigned short *) src;
		outl (le32_to_cpu(l << 16 | l2), port);
		break;
	 case 0x01:			/* Buffer 8-bit aligned */
		--count;
		
		l  = *(unsigned char *) src << 24;
		((unsigned char *) src)++;
		l |= *(unsigned short *) src << 8;
		((unsigned short *) src)++;
		while (count--)
		{
			l2 = *(unsigned int *) src;
			((unsigned int *) src)++;
			outl (le32_to_cpu(l | l2 >> 24), port);
			l = l2 << 8;
		}
		l2 = *(unsigned char *) src;
		      outl (le32_to_cpu(l | l2), port);
		break;
	 case 0x03:			/* Buffer 8-bit aligned */
		--count;
		
		l  = *(unsigned char *) src << 24;
		((unsigned char *) src)++;
		while (count--)
		{
			l2 = *(unsigned int *) src;
			((unsigned int *) src)++;
			outl (le32_to_cpu(l | l2 >> 8), port);
			l = l2 << 24;
		}
		l2  = *(unsigned short *) src << 16;
		((unsigned short *) src)++;
		l2 |= *(unsigned char *) src;
		outl (le32_to_cpu(l | l2), port);
		break;
	}
}
