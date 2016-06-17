/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Low level I/O functions for SNI.
 */
#include <linux/string.h>
#include <linux/spinlock.h>
#include <asm/addrspace.h>
#include <asm/system.h>
#include <asm/sni.h>

/*
 * Urgs...  We only can see a 16mb window of the 4gb EISA address space
 * at PCIMT_EISA_BASE.  Maladia segmentitis ...
 *
 * To avoid locking and all the related headacke we implement this such
 * that accessing the bus address space nests, so we're treating this
 * correctly even for interrupts.  This is going to suck seriously for
 * the SMP members of the RM family.
 *
 * Making things worse the PCIMT_CSMAPISA register resides on the X bus with
 * it's unbeatable 1.4 mb/s transfer rate.
 */

static inline void eisa_map(unsigned long address)
{
	unsigned char upper;

	upper = address >> 24;
	*(volatile unsigned char *)PCIMT_CSMAPISA = ~upper;
}

#define save_eisa_map()							\
	(*(volatile unsigned char *)PCIMT_CSMAPISA)
#define restore_eisa_map(val)						\
	do { (*(volatile unsigned char *)PCIMT_CSMAPISA) = val; } while(0)

static unsigned char sni_readb(unsigned long addr)
{
	unsigned char res;
	unsigned int save_map;

	save_map = save_eisa_map();
	eisa_map(addr);
	addr &= 0xffffff;
	res = *(volatile unsigned char *) (PCIMT_EISA_BASE + addr);
	restore_eisa_map(save_map);

	return res;
}

static unsigned short sni_readw(unsigned long addr)
{
	unsigned short res;
	unsigned int save_map;

	save_map = save_eisa_map();
	eisa_map(addr);
	addr &= 0xffffff;
	res = *(volatile unsigned char *) (PCIMT_EISA_BASE + addr);
	restore_eisa_map(save_map);

	return res;
}

static unsigned int sni_readl(unsigned long addr)
{
	unsigned int res;
	unsigned int save_map;

	save_map = save_eisa_map();
	eisa_map(addr);
	addr &= 0xffffff;
	res = *(volatile unsigned char *) (PCIMT_EISA_BASE + addr);
	restore_eisa_map(save_map);

	return res;
}

static void sni_writeb(unsigned char val, unsigned long addr)
{
	unsigned int save_map;

	save_map = save_eisa_map();
	eisa_map(addr);
	addr &= 0xffffff;
	*(volatile unsigned char *) (PCIMT_EISA_BASE + addr) = val;
	restore_eisa_map(save_map);
}

static void sni_writew(unsigned short val, unsigned long addr)
{
	unsigned int save_map;

	save_map = save_eisa_map();
	eisa_map(addr);
	addr &= 0xffffff;
	*(volatile unsigned char *) (PCIMT_EISA_BASE + addr) = val;
	restore_eisa_map(save_map);
}

static void sni_writel(unsigned int val, unsigned long addr)
{
	unsigned int save_map;

	save_map = save_eisa_map();
	eisa_map(addr);
	addr &= 0xffffff;
	*(volatile unsigned char *) (PCIMT_EISA_BASE + addr) = val;
	restore_eisa_map(save_map);
}

static void sni_memset_io(unsigned long addr, int val, unsigned long len)
{
	unsigned long waddr;
	unsigned int save_map;

	save_map = save_eisa_map();
	waddr = PCIMT_EISA_BASE | (addr & 0xffffff);
	while(len) {
		unsigned long fraglen;

		fraglen = (~addr + 1) & 0xffffff;
		fraglen = (fraglen < len) ? fraglen : len;
		eisa_map(addr);
		memset((char *)waddr, val, fraglen);
		addr += fraglen;
		waddr = waddr + fraglen - 0x1000000;
		len -= fraglen;
	}
	restore_eisa_map(save_map);
}

static void sni_memcpy_fromio(unsigned long to, unsigned long from, unsigned long len)
{
	unsigned long waddr;
	unsigned int save_map;

	save_map = save_eisa_map();
	waddr = PCIMT_EISA_BASE | (from & 0xffffff);
	while(len) {
		unsigned long fraglen;

		fraglen = (~from + 1) & 0xffffff;
		fraglen = (fraglen < len) ? fraglen : len;
		eisa_map(from);
		memcpy((void *)to, (void *)waddr, fraglen);
		to += fraglen;
		from += fraglen;
		waddr = waddr + fraglen - 0x1000000;
		len -= fraglen;
	}
	restore_eisa_map(save_map);
}

static void sni_memcpy_toio(unsigned long to, unsigned long from, unsigned long len)
{
	unsigned long waddr;
	unsigned int save_map;

	save_map = save_eisa_map();
	waddr = PCIMT_EISA_BASE | (to & 0xffffff);
	while(len) {
		unsigned long fraglen;

		fraglen = (~to + 1) & 0xffffff;
		fraglen = (fraglen < len) ? fraglen : len;
		eisa_map(to);
		memcpy((char *)to + PCIMT_EISA_BASE, (void *)from, fraglen);
		to += fraglen;
		from += fraglen;
		waddr = waddr + fraglen - 0x1000000;
		len -= fraglen;
	}
	restore_eisa_map(save_map);
}
