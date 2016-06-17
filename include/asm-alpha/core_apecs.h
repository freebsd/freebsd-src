#ifndef __ALPHA_APECS__H__
#define __ALPHA_APECS__H__

#include <linux/types.h>
#include <asm/compiler.h>

/*
 * APECS is the internal name for the 2107x chipset which provides
 * memory controller and PCI access for the 21064 chip based systems.
 *
 * This file is based on:
 *
 * DECchip 21071-AA and DECchip 21072-AA Core Logic Chipsets
 * Data Sheet
 *
 * EC-N0648-72
 *
 *
 * david.rusling@reo.mts.dec.com Initial Version.
 *
 */

/*
   An AVANTI *might* be an XL, and an XL has only 27 bits of ISA address
   that get passed through the PCI<->ISA bridge chip. So we've gotta use
   both windows to max out the physical memory we can DMA to. Sigh...

   If we try a window at 0 for 1GB as a work-around, we run into conflicts
   with ISA/PCI bus memory which can't be relocated, like VGA aperture and
   BIOS ROMs. So we must put the windows high enough to avoid these areas.

   We put window 1 at BUS 64Mb for 64Mb, mapping physical 0 to 64Mb-1,
   and window 2 at BUS 1Gb for 1Gb, mapping physical 0 to 1Gb-1.
   Yes, this does map 0 to 64Mb-1 twice, but only window 1 will actually
   be used for that range (via virt_to_bus()).

   Note that we actually fudge the window 1 maximum as 48Mb instead of 64Mb,
   to keep virt_to_bus() from returning an address in the first window, for
   a data area that goes beyond the 64Mb first DMA window.  Sigh...
   The fudge factor MUST match with <asm/dma.h> MAX_DMA_ADDRESS, but
   we can't just use that here, because of header file looping... :-(

   Window 1 will be used for all DMA from the ISA bus; yes, that does
   limit what memory an ISA floppy or sound card or Ethernet can touch, but
   it's also a known limitation on other platforms as well. We use the
   same technique that is used on INTEL platforms with similar limitation:
   set MAX_DMA_ADDRESS and clear some pages' DMAable flags during mem_init().
   We trust that any ISA bus device drivers will *always* ask for DMAable
   memory explicitly via kmalloc()/get_free_pages() flags arguments.

   Note that most PCI bus devices' drivers do *not* explicitly ask for
   DMAable memory; they count on being able to DMA to any memory they
   get from kmalloc()/get_free_pages(). They will also use window 1 for
   any physical memory accesses below 64Mb; the rest will be handled by
   window 2, maxing out at 1Gb of memory. I trust this is enough... :-)

   We hope that the area before the first window is large enough so that
   there will be no overlap at the top end (64Mb). We *must* locate the
   PCI cards' memory just below window 1, so that there's still the
   possibility of being able to access it via SPARSE space. This is
   important for cards such as the Matrox Millennium, whose Xserver
   wants to access memory-mapped registers in byte and short lengths.

   Note that the XL is treated differently from the AVANTI, even though
   for most other things they are identical. It didn't seem reasonable to
   make the AVANTI support pay for the limitations of the XL. It is true,
   however, that an XL kernel will run on an AVANTI without problems.

   %%% All of this should be obviated by the ability to route
   everything through the iommu.
*/

/*
 * 21071-DA Control and Status registers.
 * These are used for PCI memory access.
 */
#define APECS_IOC_DCSR                  (IDENT_ADDR + 0x1A0000000UL)
#define APECS_IOC_PEAR                  (IDENT_ADDR + 0x1A0000020UL)
#define APECS_IOC_SEAR                  (IDENT_ADDR + 0x1A0000040UL)
#define APECS_IOC_DR1                   (IDENT_ADDR + 0x1A0000060UL)
#define APECS_IOC_DR2                   (IDENT_ADDR + 0x1A0000080UL)
#define APECS_IOC_DR3                   (IDENT_ADDR + 0x1A00000A0UL)

#define APECS_IOC_TB1R                  (IDENT_ADDR + 0x1A00000C0UL)
#define APECS_IOC_TB2R                  (IDENT_ADDR + 0x1A00000E0UL)

#define APECS_IOC_PB1R                  (IDENT_ADDR + 0x1A0000100UL)
#define APECS_IOC_PB2R                  (IDENT_ADDR + 0x1A0000120UL)

#define APECS_IOC_PM1R                  (IDENT_ADDR + 0x1A0000140UL)
#define APECS_IOC_PM2R                  (IDENT_ADDR + 0x1A0000160UL)

#define APECS_IOC_HAXR0                 (IDENT_ADDR + 0x1A0000180UL)
#define APECS_IOC_HAXR1                 (IDENT_ADDR + 0x1A00001A0UL)
#define APECS_IOC_HAXR2                 (IDENT_ADDR + 0x1A00001C0UL)

#define APECS_IOC_PMLT                  (IDENT_ADDR + 0x1A00001E0UL)

#define APECS_IOC_TLBTAG0               (IDENT_ADDR + 0x1A0000200UL)
#define APECS_IOC_TLBTAG1               (IDENT_ADDR + 0x1A0000220UL)
#define APECS_IOC_TLBTAG2               (IDENT_ADDR + 0x1A0000240UL)
#define APECS_IOC_TLBTAG3               (IDENT_ADDR + 0x1A0000260UL)
#define APECS_IOC_TLBTAG4               (IDENT_ADDR + 0x1A0000280UL)
#define APECS_IOC_TLBTAG5               (IDENT_ADDR + 0x1A00002A0UL)
#define APECS_IOC_TLBTAG6               (IDENT_ADDR + 0x1A00002C0UL)
#define APECS_IOC_TLBTAG7               (IDENT_ADDR + 0x1A00002E0UL)

#define APECS_IOC_TLBDATA0              (IDENT_ADDR + 0x1A0000300UL)
#define APECS_IOC_TLBDATA1              (IDENT_ADDR + 0x1A0000320UL)
#define APECS_IOC_TLBDATA2              (IDENT_ADDR + 0x1A0000340UL)
#define APECS_IOC_TLBDATA3              (IDENT_ADDR + 0x1A0000360UL)
#define APECS_IOC_TLBDATA4              (IDENT_ADDR + 0x1A0000380UL)
#define APECS_IOC_TLBDATA5              (IDENT_ADDR + 0x1A00003A0UL)
#define APECS_IOC_TLBDATA6              (IDENT_ADDR + 0x1A00003C0UL)
#define APECS_IOC_TLBDATA7              (IDENT_ADDR + 0x1A00003E0UL)

#define APECS_IOC_TBIA                  (IDENT_ADDR + 0x1A0000400UL)


/*
 * 21071-CA Control and Status registers.
 * These are used to program memory timing,
 *  configure memory and initialise the B-Cache.
 */
#define APECS_MEM_GCR		        (IDENT_ADDR + 0x180000000UL)
#define APECS_MEM_EDSR		        (IDENT_ADDR + 0x180000040UL)
#define APECS_MEM_TAR  		        (IDENT_ADDR + 0x180000060UL)
#define APECS_MEM_ELAR		        (IDENT_ADDR + 0x180000080UL)
#define APECS_MEM_EHAR  		(IDENT_ADDR + 0x1800000a0UL)
#define APECS_MEM_SFT_RST		(IDENT_ADDR + 0x1800000c0UL)
#define APECS_MEM_LDxLAR 		(IDENT_ADDR + 0x1800000e0UL)
#define APECS_MEM_LDxHAR 		(IDENT_ADDR + 0x180000100UL)
#define APECS_MEM_GTR    		(IDENT_ADDR + 0x180000200UL)
#define APECS_MEM_RTR    		(IDENT_ADDR + 0x180000220UL)
#define APECS_MEM_VFPR   		(IDENT_ADDR + 0x180000240UL)
#define APECS_MEM_PDLDR  		(IDENT_ADDR + 0x180000260UL)
#define APECS_MEM_PDhDR  		(IDENT_ADDR + 0x180000280UL)

/* Bank x Base Address Register */
#define APECS_MEM_B0BAR  		(IDENT_ADDR + 0x180000800UL)
#define APECS_MEM_B1BAR  		(IDENT_ADDR + 0x180000820UL)
#define APECS_MEM_B2BAR  		(IDENT_ADDR + 0x180000840UL)
#define APECS_MEM_B3BAR  		(IDENT_ADDR + 0x180000860UL)
#define APECS_MEM_B4BAR  		(IDENT_ADDR + 0x180000880UL)
#define APECS_MEM_B5BAR  		(IDENT_ADDR + 0x1800008A0UL)
#define APECS_MEM_B6BAR  		(IDENT_ADDR + 0x1800008C0UL)
#define APECS_MEM_B7BAR  		(IDENT_ADDR + 0x1800008E0UL)
#define APECS_MEM_B8BAR  		(IDENT_ADDR + 0x180000900UL)

/* Bank x Configuration Register */
#define APECS_MEM_B0BCR  		(IDENT_ADDR + 0x180000A00UL)
#define APECS_MEM_B1BCR  		(IDENT_ADDR + 0x180000A20UL)
#define APECS_MEM_B2BCR  		(IDENT_ADDR + 0x180000A40UL)
#define APECS_MEM_B3BCR  		(IDENT_ADDR + 0x180000A60UL)
#define APECS_MEM_B4BCR  		(IDENT_ADDR + 0x180000A80UL)
#define APECS_MEM_B5BCR  		(IDENT_ADDR + 0x180000AA0UL)
#define APECS_MEM_B6BCR  		(IDENT_ADDR + 0x180000AC0UL)
#define APECS_MEM_B7BCR  		(IDENT_ADDR + 0x180000AE0UL)
#define APECS_MEM_B8BCR  		(IDENT_ADDR + 0x180000B00UL)

/* Bank x Timing Register A */
#define APECS_MEM_B0TRA  		(IDENT_ADDR + 0x180000C00UL)
#define APECS_MEM_B1TRA  		(IDENT_ADDR + 0x180000C20UL)
#define APECS_MEM_B2TRA  		(IDENT_ADDR + 0x180000C40UL)
#define APECS_MEM_B3TRA  		(IDENT_ADDR + 0x180000C60UL)
#define APECS_MEM_B4TRA  		(IDENT_ADDR + 0x180000C80UL)
#define APECS_MEM_B5TRA  		(IDENT_ADDR + 0x180000CA0UL)
#define APECS_MEM_B6TRA  		(IDENT_ADDR + 0x180000CC0UL)
#define APECS_MEM_B7TRA  		(IDENT_ADDR + 0x180000CE0UL)
#define APECS_MEM_B8TRA  		(IDENT_ADDR + 0x180000D00UL)

/* Bank x Timing Register B */
#define APECS_MEM_B0TRB                 (IDENT_ADDR + 0x180000E00UL)
#define APECS_MEM_B1TRB  		(IDENT_ADDR + 0x180000E20UL)
#define APECS_MEM_B2TRB  		(IDENT_ADDR + 0x180000E40UL)
#define APECS_MEM_B3TRB  		(IDENT_ADDR + 0x180000E60UL)
#define APECS_MEM_B4TRB  		(IDENT_ADDR + 0x180000E80UL)
#define APECS_MEM_B5TRB  		(IDENT_ADDR + 0x180000EA0UL)
#define APECS_MEM_B6TRB  		(IDENT_ADDR + 0x180000EC0UL)
#define APECS_MEM_B7TRB  		(IDENT_ADDR + 0x180000EE0UL)
#define APECS_MEM_B8TRB  		(IDENT_ADDR + 0x180000F00UL)


/*
 * Memory spaces:
 */
#define APECS_IACK_SC		        (IDENT_ADDR + 0x1b0000000UL)
#define APECS_CONF		        (IDENT_ADDR + 0x1e0000000UL)
#define APECS_IO			(IDENT_ADDR + 0x1c0000000UL)
#define APECS_SPARSE_MEM		(IDENT_ADDR + 0x200000000UL)
#define APECS_DENSE_MEM		        (IDENT_ADDR + 0x300000000UL)


/*
 * Bit definitions for I/O Controller status register 0:
 */
#define APECS_IOC_STAT0_CMD		0xf
#define APECS_IOC_STAT0_ERR		(1<<4)
#define APECS_IOC_STAT0_LOST		(1<<5)
#define APECS_IOC_STAT0_THIT		(1<<6)
#define APECS_IOC_STAT0_TREF		(1<<7)
#define APECS_IOC_STAT0_CODE_SHIFT	8
#define APECS_IOC_STAT0_CODE_MASK	0x7
#define APECS_IOC_STAT0_P_NBR_SHIFT	13
#define APECS_IOC_STAT0_P_NBR_MASK	0x7ffff

#define APECS_HAE_ADDRESS		APECS_IOC_HAXR1


/*
 * Data structure for handling APECS machine checks:
 */

struct el_apecs_mikasa_sysdata_mcheck
{
	unsigned long coma_gcr;
	unsigned long coma_edsr;
	unsigned long coma_ter;
	unsigned long coma_elar;
	unsigned long coma_ehar;
	unsigned long coma_ldlr;
	unsigned long coma_ldhr;
	unsigned long coma_base0;
	unsigned long coma_base1;
	unsigned long coma_base2;
	unsigned long coma_base3;
	unsigned long coma_cnfg0;
	unsigned long coma_cnfg1;
	unsigned long coma_cnfg2;
	unsigned long coma_cnfg3;
	unsigned long epic_dcsr;
	unsigned long epic_pear;
	unsigned long epic_sear;
	unsigned long epic_tbr1;
	unsigned long epic_tbr2;
	unsigned long epic_pbr1;
	unsigned long epic_pbr2;
	unsigned long epic_pmr1;
	unsigned long epic_pmr2;
	unsigned long epic_harx1;
	unsigned long epic_harx2;
	unsigned long epic_pmlt;
	unsigned long epic_tag0;
	unsigned long epic_tag1;
	unsigned long epic_tag2;
	unsigned long epic_tag3;
	unsigned long epic_tag4;
	unsigned long epic_tag5;
	unsigned long epic_tag6;
	unsigned long epic_tag7;
	unsigned long epic_data0;
	unsigned long epic_data1;
	unsigned long epic_data2;
	unsigned long epic_data3;
	unsigned long epic_data4;
	unsigned long epic_data5;
	unsigned long epic_data6;
	unsigned long epic_data7;

	unsigned long pceb_vid;
	unsigned long pceb_did;
	unsigned long pceb_revision;
	unsigned long pceb_command;
	unsigned long pceb_status;
	unsigned long pceb_latency;
	unsigned long pceb_control;
	unsigned long pceb_arbcon;
	unsigned long pceb_arbpri;

	unsigned long esc_id;
	unsigned long esc_revision;
	unsigned long esc_int0;
	unsigned long esc_int1;
	unsigned long esc_elcr0;
	unsigned long esc_elcr1;
	unsigned long esc_last_eisa;
	unsigned long esc_nmi_stat;

	unsigned long pci_ir;
	unsigned long pci_imr;
	unsigned long svr_mgr;
};

/* This for the normal APECS machines.  */
struct el_apecs_sysdata_mcheck
{
	unsigned long coma_gcr;
	unsigned long coma_edsr;
	unsigned long coma_ter;
	unsigned long coma_elar;
	unsigned long coma_ehar;
	unsigned long coma_ldlr;
	unsigned long coma_ldhr;
	unsigned long coma_base0;
	unsigned long coma_base1;
	unsigned long coma_base2;
	unsigned long coma_cnfg0;
	unsigned long coma_cnfg1;
	unsigned long coma_cnfg2;
	unsigned long epic_dcsr;
	unsigned long epic_pear;
	unsigned long epic_sear;
	unsigned long epic_tbr1;
	unsigned long epic_tbr2;
	unsigned long epic_pbr1;
	unsigned long epic_pbr2;
	unsigned long epic_pmr1;
	unsigned long epic_pmr2;
	unsigned long epic_harx1;
	unsigned long epic_harx2;
	unsigned long epic_pmlt;
	unsigned long epic_tag0;
	unsigned long epic_tag1;
	unsigned long epic_tag2;
	unsigned long epic_tag3;
	unsigned long epic_tag4;
	unsigned long epic_tag5;
	unsigned long epic_tag6;
	unsigned long epic_tag7;
	unsigned long epic_data0;
	unsigned long epic_data1;
	unsigned long epic_data2;
	unsigned long epic_data3;
	unsigned long epic_data4;
	unsigned long epic_data5;
	unsigned long epic_data6;
	unsigned long epic_data7;
};

struct el_apecs_procdata
{
	unsigned long paltemp[32];  /* PAL TEMP REGS. */
	/* EV4-specific fields */
	unsigned long exc_addr;     /* Address of excepting instruction. */
	unsigned long exc_sum;      /* Summary of arithmetic traps. */
	unsigned long exc_mask;     /* Exception mask (from exc_sum). */
	unsigned long iccsr;        /* IBox hardware enables. */
	unsigned long pal_base;     /* Base address for PALcode. */
	unsigned long hier;         /* Hardware Interrupt Enable. */
	unsigned long hirr;         /* Hardware Interrupt Request. */
	unsigned long csr;          /* D-stream fault info. */
	unsigned long dc_stat;      /* D-cache status (ECC/Parity Err). */
	unsigned long dc_addr;      /* EV3 Phys Addr for ECC/DPERR. */
	unsigned long abox_ctl;     /* ABox Control Register. */
	unsigned long biu_stat;     /* BIU Status. */
	unsigned long biu_addr;     /* BUI Address. */
	unsigned long biu_ctl;      /* BIU Control. */
	unsigned long fill_syndrome;/* For correcting ECC errors. */
	unsigned long fill_addr;    /* Cache block which was being read */
	unsigned long va;           /* Effective VA of fault or miss. */
	unsigned long bc_tag;       /* Backup Cache Tag Probe Results.*/
};


#ifdef __KERNEL__

#ifndef __EXTERN_INLINE
#define __EXTERN_INLINE extern inline
#define __IO_EXTERN_INLINE
#endif

/*
 * I/O functions:
 *
 * Unlike Jensen, the APECS machines have no concept of local
 * I/O---everything goes over the PCI bus.
 *
 * There is plenty room for optimization here.  In particular,
 * the Alpha's insb/insw/extb/extw should be useful in moving
 * data to/from the right byte-lanes.
 */

#define vip	volatile int *
#define vuip	volatile unsigned int *
#define vulp	volatile unsigned long *

__EXTERN_INLINE u8 apecs_inb(unsigned long addr)
{
	long result = *(vip) ((addr << 5) + APECS_IO + 0x00);
	return __kernel_extbl(result, addr & 3);
}

__EXTERN_INLINE void apecs_outb(u8 b, unsigned long addr)
{
	unsigned long w;

	w = __kernel_insbl(b, addr & 3);
	*(vuip) ((addr << 5) + APECS_IO + 0x00) = w;
	mb();
}

__EXTERN_INLINE u16 apecs_inw(unsigned long addr)
{
	long result = *(vip) ((addr << 5) + APECS_IO + 0x08);
	return __kernel_extwl(result, addr & 3);
}

__EXTERN_INLINE void apecs_outw(u16 b, unsigned long addr)
{
	unsigned long w;

	w = __kernel_inswl(b, addr & 3);
	*(vuip) ((addr << 5) + APECS_IO + 0x08) = w;
	mb();
}

__EXTERN_INLINE u32 apecs_inl(unsigned long addr)
{
	return *(vuip) ((addr << 5) + APECS_IO + 0x18);
}

__EXTERN_INLINE void apecs_outl(u32 b, unsigned long addr)
{
	*(vuip) ((addr << 5) + APECS_IO + 0x18) = b;
	mb();
}


/*
 * Memory functions.  64-bit and 32-bit accesses are done through
 * dense memory space, everything else through sparse space.
 */

__EXTERN_INLINE u8 apecs_readb(unsigned long addr)
{
	unsigned long result, msb;

	addr -= APECS_DENSE_MEM;
	if (addr >= (1UL << 24)) {
		msb = addr & 0xf8000000;
		addr -= msb;
		set_hae(msb);
	}
	result = *(vip) ((addr << 5) + APECS_SPARSE_MEM + 0x00);
	return __kernel_extbl(result, addr & 3);
}

__EXTERN_INLINE u16 apecs_readw(unsigned long addr)
{
	unsigned long result, msb;

	addr -= APECS_DENSE_MEM;
	if (addr >= (1UL << 24)) {
		msb = addr & 0xf8000000;
		addr -= msb;
		set_hae(msb);
	}
	result = *(vip) ((addr << 5) + APECS_SPARSE_MEM + 0x08);
	return __kernel_extwl(result, addr & 3);
}

__EXTERN_INLINE u32 apecs_readl(unsigned long addr)
{
	return (*(vuip)addr) & 0xffffffff;
}

__EXTERN_INLINE u64 apecs_readq(unsigned long addr)
{
	return *(vulp)addr;
}

__EXTERN_INLINE void apecs_writeb(u8 b, unsigned long addr)
{
	unsigned long msb;

	addr -= APECS_DENSE_MEM;
	if (addr >= (1UL << 24)) {
		msb = addr & 0xf8000000;
		addr -= msb;
		set_hae(msb);
	}
	*(vuip) ((addr << 5) + APECS_SPARSE_MEM + 0x00) = b * 0x01010101;
}

__EXTERN_INLINE void apecs_writew(u16 b, unsigned long addr)
{
	unsigned long msb;

	addr -= APECS_DENSE_MEM;
	if (addr >= (1UL << 24)) {
		msb = addr & 0xf8000000;
		addr -= msb;
		set_hae(msb);
	}
	*(vuip) ((addr << 5) + APECS_SPARSE_MEM + 0x08) = b * 0x00010001;
}

__EXTERN_INLINE void apecs_writel(u32 b, unsigned long addr)
{
	*(vuip)addr = b;
}

__EXTERN_INLINE void apecs_writeq(u64 b, unsigned long addr)
{
	*(vulp)addr = b;
}

__EXTERN_INLINE unsigned long apecs_ioremap(unsigned long addr,
					    unsigned long size
					    __attribute__((unused)))
{
	return addr + APECS_DENSE_MEM;
}

__EXTERN_INLINE void apecs_iounmap(unsigned long addr)
{
	return;
}

__EXTERN_INLINE int apecs_is_ioaddr(unsigned long addr)
{
	return addr >= IDENT_ADDR + 0x180000000UL;
}

#undef vip
#undef vuip
#undef vulp

#ifdef __WANT_IO_DEF

#define __inb(p)		apecs_inb((unsigned long)(p))
#define __inw(p)		apecs_inw((unsigned long)(p))
#define __inl(p)		apecs_inl((unsigned long)(p))
#define __outb(x,p)		apecs_outb((x),(unsigned long)(p))
#define __outw(x,p)		apecs_outw((x),(unsigned long)(p))
#define __outl(x,p)		apecs_outl((x),(unsigned long)(p))
#define __readb(a)		apecs_readb((unsigned long)(a))
#define __readw(a)		apecs_readw((unsigned long)(a))
#define __readl(a)		apecs_readl((unsigned long)(a))
#define __readq(a)		apecs_readq((unsigned long)(a))
#define __writeb(x,a)		apecs_writeb((x),(unsigned long)(a))
#define __writew(x,a)		apecs_writew((x),(unsigned long)(a))
#define __writel(x,a)		apecs_writel((x),(unsigned long)(a))
#define __writeq(x,a)		apecs_writeq((x),(unsigned long)(a))
#define __ioremap(a,s)		apecs_ioremap((unsigned long)(a),(s))
#define __iounmap(a)		apecs_iounmap((unsigned long)(a))
#define __is_ioaddr(a)		apecs_is_ioaddr((unsigned long)(a))

#define __raw_readl(a)		__readl(a)
#define __raw_readq(a)		__readq(a)
#define __raw_writel(v,a)	__writel((v),(a))
#define __raw_writeq(v,a)	__writeq((v),(a))

#endif /* __WANT_IO_DEF */

#ifdef __IO_EXTERN_INLINE
#undef __EXTERN_INLINE
#undef __IO_EXTERN_INLINE
#endif

#endif /* __KERNEL__ */

#endif /* __ALPHA_APECS__H__ */
