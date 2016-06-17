/*
**  PCI Lower Bus Adapter (LBA) manager
**
**	(c) Copyright 1999,2000 Grant Grundler
**	(c) Copyright 1999,2000 Hewlett-Packard Company
**
**	This program is free software; you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**      the Free Software Foundation; either version 2 of the License, or
**      (at your option) any later version.
**
**
** This module primarily provides access to PCI bus (config/IOport
** spaces) on platforms with an SBA/LBA chipset. A/B/C/J/L/N-class
** with 4 digit model numbers - eg C3000 (and A400...sigh).
**
** LBA driver isn't as simple as the Dino driver because:
**   (a) this chip has substantial bug fixes between revisions
**       (Only one Dino bug has a software workaround :^(  )
**   (b) has more options which we don't (yet) support (DMA hints, OLARD)
**   (c) IRQ support lives in the I/O SAPIC driver (not with PCI driver)
**   (d) play nicely with both PAT and "Legacy" PA-RISC firmware (PDC).
**       (dino only deals with "Legacy" PDC)
**
** LBA driver passes the I/O SAPIC HPA to the I/O SAPIC driver.
** (I/O SAPIC is integratd in the LBA chip).
**
** FIXME: Add support to SBA and LBA drivers for DMA hint sets
** FIXME: Add support for PCI card hot-plug (OLARD).
*/

#include <linux/delay.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/init.h>		/* for __init and __devinit */
/* #define PCI_DEBUG	enable ASSERT */
#include <linux/pci.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/smp_lock.h>

#include <asm/byteorder.h>
#include <asm/irq.h>		/* for struct irq_region support */
#include <asm/pdc.h>
#include <asm/page.h>
#include <asm/segment.h>
#include <asm/system.h>

#include <asm/hardware.h>	/* for register_parisc_driver() stuff */
#include <asm/iosapic.h>	/* for iosapic_register() */
#include <asm/io.h>		/* read/write stuff */

#ifndef TRUE
#define TRUE (1 == 1)
#define FALSE (1 == 0)
#endif

#undef DEBUG_LBA	/* general stuff */
#undef DEBUG_LBA_PORT	/* debug I/O Port access */
#undef DEBUG_LBA_CFG	/* debug Config Space Access (ie PCI Bus walk) */
#undef DEBUG_LBA_PAT	/* debug PCI Resource Mgt code - PDC PAT only */

#undef FBB_SUPPORT	/* Fast Back-Back xfers - NOT READY YET */


#ifdef DEBUG_LBA
#define DBG(x...)	printk(x)
#else
#define DBG(x...)
#endif

#ifdef DEBUG_LBA_PORT
#define DBG_PORT(x...)	printk(x)
#else
#define DBG_PORT(x...)
#endif

#ifdef DEBUG_LBA_CFG
#define DBG_CFG(x...)	printk(x)
#else
#define DBG_CFG(x...)
#endif

#ifdef DEBUG_LBA_PAT
#define DBG_PAT(x...)	printk(x)
#else
#define DBG_PAT(x...)
#endif

/*
** Config accessor functions only pass in the 8-bit bus number and not
** the 8-bit "PCI Segment" number. Each LBA will be assigned a PCI bus
** number based on what firmware wrote into the scratch register.
**
** The "secondary" bus number is set to this before calling
** pci_register_ops(). If any PPB's are present, the scan will
** discover them and update the "secondary" and "subordinate"
** fields in the pci_bus structure.
**
** Changes in the configuration *may* result in a different
** bus number for each LBA depending on what firmware does.
*/

#define MODULE_NAME "lba"

#define LBA_FUNC_ID	0x0000	/* function id */
#define LBA_FCLASS	0x0008	/* function class, bist, header, rev... */
#define LBA_CAPABLE	0x0030	/* capabilities register */

#define LBA_PCI_CFG_ADDR	0x0040	/* poke CFG address here */
#define LBA_PCI_CFG_DATA	0x0048	/* read or write data here */

#define LBA_PMC_MTLT	0x0050	/* Firmware sets this - read only. */
#define LBA_FW_SCRATCH	0x0058	/* Firmware writes the PCI bus number here. */
#define LBA_ERROR_ADDR	0x0070	/* On error, address gets logged here */

#define LBA_ARB_MASK	0x0080	/* bit 0 enable arbitration. PAT/PDC enables */
#define LBA_ARB_PRI	0x0088	/* firmware sets this. */
#define LBA_ARB_MODE	0x0090	/* firmware sets this. */
#define LBA_ARB_MTLT	0x0098	/* firmware sets this. */

#define LBA_MOD_ID	0x0100	/* Module ID. PDC_PAT_CELL reports 4 */

#define LBA_STAT_CTL	0x0108	/* Status & Control */
#define   LBA_BUS_RESET		0x01	/*  Deassert PCI Bus Reset Signal */
#define   CLEAR_ERRLOG		0x10	/*  "Clear Error Log" cmd */
#define   CLEAR_ERRLOG_ENABLE	0x20	/*  "Clear Error Log" Enable */
#define   HF_ENABLE	0x40	/*    enable HF mode (default is -1 mode) */

#define LBA_LMMIO_BASE	0x0200	/* < 4GB I/O address range */
#define LBA_LMMIO_MASK	0x0208

#define LBA_GMMIO_BASE	0x0210	/* > 4GB I/O address range */
#define LBA_GMMIO_MASK	0x0218

#define LBA_WLMMIO_BASE	0x0220	/* All < 4GB ranges under the same *SBA* */
#define LBA_WLMMIO_MASK	0x0228

#define LBA_WGMMIO_BASE	0x0230	/* All > 4GB ranges under the same *SBA* */
#define LBA_WGMMIO_MASK	0x0238

#define LBA_IOS_BASE	0x0240	/* I/O port space for this LBA */
#define LBA_IOS_MASK	0x0248

#define LBA_ELMMIO_BASE	0x0250	/* Extra LMMIO range */
#define LBA_ELMMIO_MASK	0x0258

#define LBA_EIOS_BASE	0x0260	/* Extra I/O port space */
#define LBA_EIOS_MASK	0x0268

#define LBA_DMA_CTL	0x0278	/* firmware sets this */

#define LBA_IBASE	0x0300	/* SBA DMA support */
#define LBA_IMASK	0x0308

/* FIXME: ignore DMA Hint stuff until we can measure performance */
#define LBA_HINT_CFG	0x0310
#define LBA_HINT_BASE	0x0380	/* 14 registers at every 8 bytes. */

/* ERROR regs are needed for config cycle kluges */
#define LBA_ERROR_CONFIG 0x0680
#define     LBA_SMART_MODE 0x20
#define LBA_ERROR_STATUS 0x0688
#define LBA_ROPE_CTL     0x06A0

#define LBA_IOSAPIC_BASE	0x800 /* Offset of IRQ logic */

/* non-postable I/O port space, densely packed */
#ifdef __LP64__
#define LBA_ASTRO_PORT_BASE	(0xfffffffffee00000UL)
#else
#define LBA_ASTRO_PORT_BASE	(0xfee00000UL)
#endif


/*
** lba_device: Per instance Elroy data structure
*/
struct lba_device {
	struct pci_hba_data hba;

	spinlock_t	lba_lock;
	void		*iosapic_obj;

#ifdef __LP64__
	unsigned long	lmmio_base;  /* PA_VIEW - fixup MEM addresses */
	unsigned long	gmmio_base;  /* PA_VIEW - Not used (yet) */
	unsigned long	iop_base;    /* PA_VIEW - for IO port accessor funcs */
#endif

	int		flags;       /* state/functionality enabled */
	int		hw_rev;      /* HW revision of chip */
};


static u32 lba_t32;

/*
** lba "flags"
*/
#define LBA_FLAG_NO_DMA_DURING_CFG	0x01
#define LBA_FLAG_SKIP_PROBE	0x10

/* Tape Release 4 == hw_rev 5 */
#define LBA_TR4PLUS(d)      ((d)->hw_rev > 0x4)
#define LBA_DMA_DURING_CFG_DISABLED(d) ((d)->flags & LBA_FLAG_NO_DMA_DURING_CFG)
#define LBA_SKIP_PROBE(d) ((d)->flags & LBA_FLAG_SKIP_PROBE)


/* Looks nice and keeps the compiler happy */
#define LBA_DEV(d) ((struct lba_device *) (d))


/*
** Only allow 8 subsidiary busses per LBA
** Problem is the PCI bus numbering is globally shared.
*/
#define LBA_MAX_NUM_BUSES 8

/************************************
 * LBA register read and write support
 *
 * BE WARNED: register writes are posted.
 *  (ie follow writes which must reach HW with a read)
 */
#define READ_U8(addr)  __raw_readb(addr)
#define READ_U16(addr) __raw_readw(addr)
#define READ_U32(addr) __raw_readl(addr)
#define WRITE_U8(value, addr)  __raw_writeb(value, addr)
#define WRITE_U16(value, addr) __raw_writew(value, addr)
#define WRITE_U32(value, addr) __raw_writel(value, addr)

#define READ_REG8(addr)  readb(addr)
#define READ_REG16(addr) readw(addr)
#define READ_REG32(addr) readl(addr)
#define READ_REG64(addr) readq(addr)
#define WRITE_REG8(value, addr)  writeb(value, addr)
#define WRITE_REG16(value, addr) writew(value, addr)
#define WRITE_REG32(value, addr) writel(value, addr)


#define LBA_CFG_TOK(bus,dfn) ((u32) ((bus)<<16 | (dfn)<<8))
#define LBA_CFG_BUS(tok)  ((u8) ((tok)>>16))
#define LBA_CFG_DEV(tok)  ((u8) ((tok)>>11) & 0x1f)
#define LBA_CFG_FUNC(tok) ((u8) ((tok)>>8 ) & 0x7)


/*
** Extract LBA (Rope) number from HPA
** REVISIT: 16 ropes for Stretch/Ike?
*/
#define ROPES_PER_SBA	8
#define LBA_NUM(x)    ((((unsigned long) x) >> 13) & (ROPES_PER_SBA-1))


static void
lba_dump_res(struct resource *r, int d)
{
	int i;

	if (NULL == r)
		return;

	printk(KERN_DEBUG "(%p)", r->parent);
	for (i = d; i ; --i) printk(" ");
	printk(KERN_DEBUG "%p [%lx,%lx]/%x\n", r, r->start, r->end, (int) r->flags);
	lba_dump_res(r->child, d+2);
	lba_dump_res(r->sibling, d);
}


/*
** LBA rev 2.0, 2.1, 2.2, and 3.0 bus walks require a complex
** workaround for cfg cycles:
**	-- preserve  LBA state
**	-- LBA_FLAG_NO_DMA_DURING_CFG workaround
**	-- turn on smart mode
**	-- probe with config writes before doing config reads
**	-- check ERROR_STATUS
**	-- clear ERROR_STATUS
**	-- restore LBA state
**
** The workaround is only used for device discovery.
*/

static int
lba_device_present( u8 bus, u8 dfn, struct lba_device *d)
{
	u8 first_bus = d->hba.hba_bus->secondary;
	u8 last_sub_bus = d->hba.hba_bus->subordinate;
#if 0
/* FIXME - see below in this function */
        u8 dev = PCI_SLOT(dfn);
        u8 func = PCI_FUNC(dfn);
#endif

	ASSERT(bus >= first_bus);
	ASSERT(bus <= last_sub_bus);
	ASSERT((bus - first_bus) < LBA_MAX_NUM_BUSES);

	if ((bus < first_bus) ||
	    (bus > last_sub_bus) ||
	    ((bus - first_bus) >= LBA_MAX_NUM_BUSES))
	{
	    /* devices that fall into any of these cases won't get claimed */
	    return(FALSE);
	}

#if 0
/*
** FIXME: Need to implement code to fill the devices bitmap based
** on contents of the local pci_bus tree "data base".
** pci_register_ops() walks the bus for us and builds the tree.
** For now, always do the config cycle.
*/
	bus -= first_bus;

	return (((d->devices[bus][dev]) >> func) & 0x1);
#else
	return TRUE;
#endif
}



#define LBA_CFG_SETUP(d, tok) {				\
    /* Save contents of error config register.  */			\
    error_config = READ_REG32(d->hba.base_addr + LBA_ERROR_CONFIG);		\
\
    /* Save contents of status control register.  */			\
    status_control = READ_REG32(d->hba.base_addr + LBA_STAT_CTL);		\
\
    /* For LBA rev 2.0, 2.1, 2.2, and 3.0, we must disable DMA		\
    ** arbitration for full bus walks.					\
    */									\
    if (LBA_DMA_DURING_CFG_DISABLED(d)) {				\
	/* Save contents of arb mask register. */			\
	arb_mask = READ_REG32(d->hba.base_addr + LBA_ARB_MASK);		\
\
	/*								\
	 * Turn off all device arbitration bits (i.e. everything	\
	 * except arbitration enable bit).				\
	 */								\
	WRITE_REG32(0x1, d->hba.base_addr + LBA_ARB_MASK);			\
    }									\
\
    /*									\
     * Set the smart mode bit so that master aborts don't cause		\
     * LBA to go into PCI fatal mode (required).			\
     */									\
    WRITE_REG32(error_config | LBA_SMART_MODE, d->hba.base_addr + LBA_ERROR_CONFIG);	\
}


#define LBA_CFG_PROBE(d, tok) {				\
    /*									\
     * Setup Vendor ID write and read back the address register		\
     * to make sure that LBA is the bus master.				\
     */									\
    WRITE_REG32(tok | PCI_VENDOR_ID, (d)->hba.base_addr + LBA_PCI_CFG_ADDR);\
    /*									\
     * Read address register to ensure that LBA is the bus master,	\
     * which implies that DMA traffic has stopped when DMA arb is off.	\
     */									\
    lba_t32 = READ_REG32((d)->hba.base_addr + LBA_PCI_CFG_ADDR);		\
    /*									\
     * Generate a cfg write cycle (will have no affect on		\
     * Vendor ID register since read-only).				\
     */									\
    WRITE_REG32(~0, (d)->hba.base_addr + LBA_PCI_CFG_DATA);		\
    /*									\
     * Make sure write has completed before proceeding further,		\
     * i.e. before setting clear enable.				\
     */									\
    lba_t32 = READ_REG32((d)->hba.base_addr + LBA_PCI_CFG_ADDR);		\
}


/*
 * HPREVISIT:
 *   -- Can't tell if config cycle got the error.
 *
 *		OV bit is broken until rev 4.0, so can't use OV bit and
 *		LBA_ERROR_LOG_ADDR to tell if error belongs to config cycle.
 *
 *		As of rev 4.0, no longer need the error check.
 *
 *   -- Even if we could tell, we still want to return -1
 *	for **ANY** error (not just master abort).
 *
 *   -- Only clear non-fatal errors (we don't want to bring
 *	LBA out of pci-fatal mode).
 *
 *		Actually, there is still a race in which
 *		we could be clearing a fatal error.  We will
 *		live with this during our initial bus walk
 *		until rev 4.0 (no driver activity during
 *		initial bus walk).  The initial bus walk
 *		has race conditions concerning the use of
 *		smart mode as well.
 */

#define LBA_MASTER_ABORT_ERROR 0xc
#define LBA_FATAL_ERROR 0x10

#define LBA_CFG_MASTER_ABORT_CHECK(d, base, tok, error) {		\
    u32 error_status = 0;						\
    /*									\
     * Set clear enable (CE) bit. Unset by HW when new			\
     * errors are logged -- LBA HW ERS section 14.3.3).		\
     */									\
    WRITE_REG32(status_control | CLEAR_ERRLOG_ENABLE, base + LBA_STAT_CTL); \
    error_status = READ_REG32(base + LBA_ERROR_STATUS);		\
    if ((error_status & 0x1f) != 0) {					\
	/*								\
	 * Fail the config read request.				\
	 */								\
	error = 1;							\
	if ((error_status & LBA_FATAL_ERROR) == 0) {			\
	    /*								\
	     * Clear error status (if fatal bit not set) by setting	\
	     * clear error log bit (CL).				\
	     */								\
	    WRITE_REG32(status_control | CLEAR_ERRLOG, base + LBA_STAT_CTL); \
	}								\
    }									\
}

#define LBA_CFG_TR4_ADDR_SETUP(d, addr) \
    WRITE_REG32(((addr) & ~3), (d)->hba.base_addr + LBA_PCI_CFG_ADDR)

#define LBA_CFG_ADDR_SETUP(d, addr) {				\
    WRITE_REG32(((addr) & ~3), (d)->hba.base_addr + LBA_PCI_CFG_ADDR);	\
    /*									\
     * HPREVISIT:							\
     *       --	Potentially could skip this once DMA bug fixed.		\
     *									\
     * Read address register to ensure that LBA is the bus master,	\
     * which implies that DMA traffic has stopped when DMA arb is off.	\
     */									\
    lba_t32 = READ_REG32((d)->hba.base_addr + LBA_PCI_CFG_ADDR);		\
}


#define LBA_CFG_RESTORE(d, base) {					\
    /*									\
     * Restore status control register (turn off clear enable).		\
     */									\
    WRITE_REG32(status_control, base + LBA_STAT_CTL);			\
    /*									\
     * Restore error config register (turn off smart mode).		\
     */									\
    WRITE_REG32(error_config, base + LBA_ERROR_CONFIG);			\
    if (LBA_DMA_DURING_CFG_DISABLED(d)) {				\
	/*								\
	 * Restore arb mask register (reenables DMA arbitration).	\
	 */								\
	WRITE_REG32(arb_mask, base + LBA_ARB_MASK);			\
    }									\
}



static unsigned int
lba_rd_cfg(struct lba_device *d, u32 tok, u8 reg, u32 size)
{
	u32 data = ~0;
	int error = 0;
	u32 arb_mask = 0;	/* used by LBA_CFG_SETUP/RESTORE */
	u32 error_config = 0;	/* used by LBA_CFG_SETUP/RESTORE */
	u32 status_control = 0;	/* used by LBA_CFG_SETUP/RESTORE */

	ASSERT((size == sizeof(u8)) ||
		(size == sizeof(u16)) ||
		(size == sizeof(u32)));

	if ((size != sizeof(u8)) &&
		(size != sizeof(u16)) &&
		(size != sizeof(u32))) {
		return(data);
	}

	LBA_CFG_SETUP(d, tok);
	LBA_CFG_PROBE(d, tok);
	LBA_CFG_MASTER_ABORT_CHECK(d, d->hba.base_addr, tok, error);
	if (!error) {
		LBA_CFG_ADDR_SETUP(d, tok | reg);
		switch (size) {
		case sizeof(u8):
			data = (u32) READ_REG8(d->hba.base_addr + LBA_PCI_CFG_DATA + (reg & 3));
			break;
		case sizeof(u16):
			data = (u32) READ_REG16(d->hba.base_addr + LBA_PCI_CFG_DATA + (reg & 2));
			break;
		case sizeof(u32):
			data = READ_REG32(d->hba.base_addr + LBA_PCI_CFG_DATA);
			break;
		default:
			break; /* leave data as -1 */
		}
	}
	LBA_CFG_RESTORE(d, d->hba.base_addr);
	return(data);
}


#define LBA_CFG_RD(size, mask) \
static int lba_cfg_read##size (struct pci_dev *dev, int pos, u##size *data) \
{ \
	struct lba_device *d = LBA_DEV(dev->bus->sysdata); \
	u32 local_bus = (dev->bus->parent == NULL) ? 0 : dev->bus->secondary; \
	u32 tok = LBA_CFG_TOK(local_bus,dev->devfn); \
 \
/* FIXME: B2K/C3600 workaround is always use old method... */ \
	/* if (!LBA_TR4PLUS(d) && !LBA_SKIP_PROBE(d)) */ { \
		/* original - Generate config cycle on broken elroy \
		  with risk we will miss PCI bus errors. */ \
		*data = (u##size) lba_rd_cfg(d, tok, pos, sizeof(u##size)); \
		DBG_CFG("%s(%s+%2x) -> 0x%x (a)\n", __FUNCTION__, dev->slot_name, pos, *data); \
		return(*data == (u##size) -1); \
	} \
 \
	if (LBA_SKIP_PROBE(d) && (!lba_device_present(dev->bus->secondary, dev->devfn, d))) \
	{ \
		DBG_CFG("%s(%s+%2x) -> -1 (b)\n", __FUNCTION__, dev->slot_name, pos); \
		/* either don't want to look or know device isn't present. */ \
		*data = (u##size) -1; \
		return(0); \
	} \
 \
	/* Basic Algorithm \
	** Should only get here on fully working LBA rev. \
	** This is how simple the code should have been. \
	*/ \
	LBA_CFG_TR4_ADDR_SETUP(d, tok | pos); \
	*data = READ_REG##size(d->hba.base_addr + LBA_PCI_CFG_DATA + (pos & mask));\
	DBG_CFG("%s(%s+%2x) -> 0x%x (c)\n", __FUNCTION__, dev->slot_name, pos, *data);\
	return(*data == (u##size) -1); \
}

LBA_CFG_RD( 8, 3) 
LBA_CFG_RD(16, 2) 
LBA_CFG_RD(32, 0) 



static void
lba_wr_cfg( struct lba_device *d, u32 tok, u8 reg, u32 data, u32 size)
{
	int error = 0;
	u32 arb_mask = 0;
	u32 error_config = 0;
	u32 status_control = 0;

	ASSERT((size == sizeof(u8)) ||
		(size == sizeof(u16)) ||
		(size == sizeof(u32)));

	if ((size != sizeof(u8)) &&
		(size != sizeof(u16)) &&
		(size != sizeof(u32))) {
			return;
	}

	LBA_CFG_SETUP(d, tok);
	LBA_CFG_ADDR_SETUP(d, tok | reg);
	switch (size) {
	case sizeof(u8):
		WRITE_REG8((u8) data, d->hba.base_addr + LBA_PCI_CFG_DATA + (reg&3));
		break;
	case sizeof(u16):
		WRITE_REG16((u8) data, d->hba.base_addr + LBA_PCI_CFG_DATA +(reg&2));
		break;
	case sizeof(u32):
		WRITE_REG32(data, d->hba.base_addr + LBA_PCI_CFG_DATA);
		break;
	default:
		break;
	}
	LBA_CFG_MASTER_ABORT_CHECK(d, d->hba.base_addr, tok, error);
	LBA_CFG_RESTORE(d, d->hba.base_addr);
}


/*
 * LBA 4.0 config write code implements non-postable semantics
 * by doing a read of CONFIG ADDR after the write.
 */

#define LBA_CFG_WR(size, mask) \
static int lba_cfg_write##size (struct pci_dev *dev, int pos, u##size data) \
{ \
	struct lba_device *d = LBA_DEV(dev->bus->sysdata); \
	u32 local_bus = (dev->bus->parent == NULL) ? 0 : dev->bus->secondary; \
	u32 tok = LBA_CFG_TOK(local_bus,dev->devfn); \
 \
 	ASSERT((tok & 0xff) == 0); \
	ASSERT(pos < 0x100); \
 \
	if (!LBA_TR4PLUS(d) && !LBA_SKIP_PROBE(d)) { \
		/* Original Workaround */ \
		lba_wr_cfg(d, tok, pos, (u32) data, sizeof(u##size)); \
		DBG_CFG("%s(%s+%2x) = 0x%x (a)\n", __FUNCTION__, dev->slot_name, pos, data); \
		return 0; \
	} \
 \
	if (LBA_SKIP_PROBE(d) && (!lba_device_present(dev->bus->secondary, dev->devfn, d))) { \
		DBG_CFG("%s(%s+%2x) = 0x%x (b)\n", __FUNCTION__, dev->slot_name, pos, data); \
		return 1; /* New Workaround */ \
	} \
 \
	DBG_CFG("%s(%s+%2x) = 0x%x (c)\n", __FUNCTION__, dev->slot_name, pos, data); \
	/* Basic Algorithm */ \
	LBA_CFG_TR4_ADDR_SETUP(d, tok | pos); \
	WRITE_REG##size(data, d->hba.base_addr + LBA_PCI_CFG_DATA + (pos & mask)); \
	lba_t32 = READ_REG32(d->hba.base_addr + LBA_PCI_CFG_ADDR); \
	return 0; \
}


LBA_CFG_WR( 8, 3) 
LBA_CFG_WR(16, 2) 
LBA_CFG_WR(32, 0) 

static struct pci_ops lba_cfg_ops = {
	read_byte:	lba_cfg_read8,
	read_word:	lba_cfg_read16,
	read_dword:	lba_cfg_read32,
	write_byte:	lba_cfg_write8,
	write_word:	lba_cfg_write16,
	write_dword:	lba_cfg_write32
};



static void
lba_bios_init(void)
{
	DBG(MODULE_NAME ": lba_bios_init\n");
}


#ifdef __LP64__

/*
** Determine if a device is already configured.
** If so, reserve it resources.
**
** Read PCI cfg command register and see if I/O or MMIO is enabled.
** PAT has to enable the devices it's using.
**
** Note: resources are fixed up before we try to claim them.
*/
static void
lba_claim_dev_resources(struct pci_dev *dev)
{
	u16 cmd;
	int i, srch_flags;

	(void) lba_cfg_read16(dev, PCI_COMMAND, &cmd);

	srch_flags  = (cmd & PCI_COMMAND_IO) ? IORESOURCE_IO : 0;
	if (cmd & PCI_COMMAND_MEMORY)
		srch_flags |= IORESOURCE_MEM;

	if (!srch_flags)
		return;

	for (i = 0; i <= PCI_ROM_RESOURCE; i++) {
		if (dev->resource[i].flags & srch_flags) {
			pci_claim_resource(dev, i);
			DBG("   claimed %s %d [%lx,%lx]/%x\n",
				dev->slot_name, i,
				dev->resource[i].start,
				dev->resource[i].end,
				(int) dev->resource[i].flags
				);
		}
	}
}
#endif


/*
** The algorithm is generic code.
** But it needs to access local data structures to get the IRQ base.
** Could make this a "pci_fixup_irq(bus, region)" but not sure
** it's worth it.
**
** Called by do_pci_scan_bus() immediately after each PCI bus is walked.
** Resources aren't allocated until recursive buswalk below HBA is completed.
*/
static void
lba_fixup_bus(struct pci_bus *bus)
{
	struct list_head *ln;
#ifdef FBB_SUPPORT
	u16 fbb_enable = PCI_STATUS_FAST_BACK;
	u16 status;
#endif
	struct lba_device *ldev = LBA_DEV(bus->sysdata);
	int lba_portbase = HBA_PORT_BASE(ldev->hba.hba_num);

	DBG("lba_fixup_bus(0x%p) bus %d sysdata 0x%p\n",
		bus, bus->secondary, bus->sysdata);

	/*
	** Properly Setup MMIO resources for this bus.
	** pci_alloc_primary_bus() mangles this.
	*/
	if (NULL == bus->self) {
		int err;

		DBG("lba_fixup_bus() %s [%lx/%lx]/%x\n",
			ldev->hba.io_space.name,
			ldev->hba.io_space.start, ldev->hba.io_space.end,
			(int) ldev->hba.io_space.flags);
		DBG("lba_fixup_bus() %s [%lx/%lx]/%x\n",
			ldev->hba.lmmio_space.name,
			ldev->hba.lmmio_space.start, ldev->hba.lmmio_space.end,
			(int) ldev->hba.lmmio_space.flags);

		err = request_resource(&ioport_resource, &(ldev->hba.io_space));
		if (err < 0) {
			BUG();
			lba_dump_res(&ioport_resource, 2);
		}
		err = request_resource(&iomem_resource, &(ldev->hba.lmmio_space));
		if (err < 0) {
			BUG();
			lba_dump_res(&iomem_resource, 2);
		}

		bus->resource[0] = &(ldev->hba.io_space);
		bus->resource[1] = &(ldev->hba.lmmio_space);
	} else {
		pci_read_bridge_bases(bus);

		/* Turn off downstream PreFetchable Memory range by default */
		bus->resource[2]->start = 0;
		bus->resource[2]->end = 0;
	}


	list_for_each(ln, &bus->devices) {
		int i;
		struct pci_dev *dev = pci_dev_b(ln);

		DBG("lba_fixup_bus() %s\n", dev->name);

		/* Virtualize Device/Bridge Resources. */
		for (i = 0; i < PCI_NUM_RESOURCES; i++) {
			struct resource *res = &dev->resource[i];

			/* If resource not allocated - skip it */
			if (!res->start)
				continue;

			if (res->flags & IORESOURCE_IO) {
				DBG("lba_fixup_bus() I/O Ports [%lx/%lx] -> ",
					res->start, res->end);
				res->start |= lba_portbase;
				res->end   |= lba_portbase;
				DBG("[%lx/%lx]\n", res->start, res->end);
			} else if (res->flags & IORESOURCE_MEM) {
				/*
				** Convert PCI (IO_VIEW) addresses to
				** processor (PA_VIEW) addresses
				 */
				DBG("lba_fixup_bus() MMIO [%lx/%lx] -> ",
					res->start, res->end);
				res->start = PCI_HOST_ADDR(HBA_DATA(ldev), res->start);
				res->end   = PCI_HOST_ADDR(HBA_DATA(ldev), res->end);
				DBG("[%lx/%lx]\n", res->start, res->end);
			}
		}

#ifdef FBB_SUPPORT
		/*
		** If one device does not support FBB transfers,
		** No one on the bus can be allowed to use them.
		*/
		(void) lba_cfg_read16(dev, PCI_STATUS, &status);
		fbb_enable &= status;
#endif

#ifdef __LP64__
		if (is_pdc_pat()) {
			/* Claim resources for PDC's devices */
			lba_claim_dev_resources(dev);
		}
#endif

                /*
		** P2PB's have no IRQs. ignore them.
		*/
		if ((dev->class >> 8) == PCI_CLASS_BRIDGE_PCI)
			continue;

		/* Adjust INTERRUPT_LINE for this dev */
		iosapic_fixup_irq(ldev->iosapic_obj, dev);
	}

#ifdef FBB_SUPPORT
/* FIXME/REVISIT - finish figuring out to set FBB on both
** pci_setup_bridge() clobbers PCI_BRIDGE_CONTROL.
** Can't fixup here anyway....garr...
*/
	if (fbb_enable) {
		if (bus->self) {
			u8 control;
			/* enable on PPB */
			(void) lba_cfg_read8(bus->self, PCI_BRIDGE_CONTROL, &control);
			(void) lba_cfg_write8(bus->self, PCI_BRIDGE_CONTROL, control | PCI_STATUS_FAST_BACK);

		} else {
			/* enable on LBA */
		}
		fbb_enable = PCI_COMMAND_FAST_BACK;
	}

	/* Lastly enable FBB/PERR/SERR on all devices too */
	list_for_each(ln, &bus->devices) {
		(void) lba_cfg_read16(dev, PCI_COMMAND, &status);
		status |= PCI_COMMAND_PARITY | PCI_COMMAND_SERR | fbb_enable;
		(void) lba_cfg_write16(dev, PCI_COMMAND, status);
	}
#endif
}


struct pci_bios_ops lba_bios_ops = {
	init:		lba_bios_init,
	fixup_bus:	lba_fixup_bus,
};




/*******************************************************
**
** LBA Sprockets "I/O Port" Space Accessor Functions
**
** This set of accessor functions is intended for use with
** "legacy firmware" (ie Sprockets on Allegro/Forte boxes).
**
** Many PCI devices don't require use of I/O port space (eg Tulip,
** NCR720) since they export the same registers to both MMIO and
** I/O port space. In general I/O port space is slower than
** MMIO since drivers are designed so PIO writes can be posted.
**
********************************************************/

#define LBA_PORT_IN(size, mask) \
static u##size lba_astro_in##size (struct pci_hba_data *d, u16 addr) \
{ \
	u##size t; \
	t = READ_REG##size(LBA_ASTRO_PORT_BASE + addr); \
	DBG_PORT(" 0x%x\n", t); \
	return (t); \
}

LBA_PORT_IN( 8, 3)
LBA_PORT_IN(16, 2)
LBA_PORT_IN(32, 0)



/*
** BUG X4107:  Ordering broken - DMA RD return can bypass PIO WR
**
** Fixed in Elroy 2.2. The READ_U32(..., LBA_FUNC_ID) below is
** guarantee non-postable completion semantics - not avoid X4107.
** The READ_U32 only guarantees the write data gets to elroy but
** out to the PCI bus. We can't read stuff from I/O port space
** since we don't know what has side-effects. Attempting to read
** from configuration space would be suicidal given the number of
** bugs in that elroy functionality.
**
**      Description:
**          DMA read results can improperly pass PIO writes (X4107).  The
**          result of this bug is that if a processor modifies a location in
**          memory after having issued PIO writes, the PIO writes are not
**          guaranteed to be completed before a PCI device is allowed to see
**          the modified data in a DMA read.
**
**          Note that IKE bug X3719 in TR1 IKEs will result in the same
**          symptom.
**
**      Workaround:
**          The workaround for this bug is to always follow a PIO write with
**          a PIO read to the same bus before starting DMA on that PCI bus.
**
*/
#define LBA_PORT_OUT(size, mask) \
static void lba_astro_out##size (struct pci_hba_data *d, u16 addr, u##size val) \
{ \
	ASSERT(d != NULL); \
	DBG_PORT("%s(0x%p, 0x%x, 0x%x)\n", __FUNCTION__, d, addr, val); \
	WRITE_REG##size(val, LBA_ASTRO_PORT_BASE + addr); \
	if (LBA_DEV(d)->hw_rev < 3) \
		lba_t32 = READ_U32(d->base_addr + LBA_FUNC_ID); \
}

LBA_PORT_OUT( 8, 3)
LBA_PORT_OUT(16, 2)
LBA_PORT_OUT(32, 0)


static struct pci_port_ops lba_astro_port_ops = {
	inb:	lba_astro_in8,
	inw:	lba_astro_in16,
	inl:	lba_astro_in32,
	outb:	lba_astro_out8,
	outw:	lba_astro_out16,
	outl:	lba_astro_out32
};


#ifdef __LP64__
#define PIOP_TO_GMMIO(lba, addr) \
	((lba)->iop_base + (((addr)&0xFFFC)<<10) + ((addr)&3))

/*******************************************************
**
** LBA PAT "I/O Port" Space Accessor Functions
**
** This set of accessor functions is intended for use with
** "PAT PDC" firmware (ie Prelude/Rhapsody/Piranha boxes).
**
** This uses the PIOP space located in the first 64MB of GMMIO.
** Each rope gets a full 64*KB* (ie 4 bytes per page) this way.
** bits 1:0 stay the same.  bits 15:2 become 25:12.
** Then add the base and we can generate an I/O Port cycle.
********************************************************/
#undef LBA_PORT_IN
#define LBA_PORT_IN(size, mask) \
static u##size lba_pat_in##size (struct pci_hba_data *l, u16 addr) \
{ \
	u##size t; \
	ASSERT(bus != NULL); \
	DBG_PORT("%s(0x%p, 0x%x) ->", __FUNCTION__, l, addr); \
	t = READ_REG##size(PIOP_TO_GMMIO(LBA_DEV(l), addr)); \
	DBG_PORT(" 0x%x\n", t); \
	return (t); \
}

LBA_PORT_IN( 8, 3)
LBA_PORT_IN(16, 2)
LBA_PORT_IN(32, 0)


#undef LBA_PORT_OUT
#define LBA_PORT_OUT(size, mask) \
static void lba_pat_out##size (struct pci_hba_data *l, u16 addr, u##size val) \
{ \
	void *where = (void *) PIOP_TO_GMMIO(LBA_DEV(l), addr); \
	ASSERT(bus != NULL); \
	DBG_PORT("%s(0x%p, 0x%x, 0x%x)\n", __FUNCTION__, l, addr, val); \
	WRITE_REG##size(val, where); \
	/* flush the I/O down to the elroy at least */ \
	lba_t32 = READ_U32(l->base_addr + LBA_FUNC_ID); \
}

LBA_PORT_OUT( 8, 3)
LBA_PORT_OUT(16, 2)
LBA_PORT_OUT(32, 0)


static struct pci_port_ops lba_pat_port_ops = {
	inb:	lba_pat_in8,
	inw:	lba_pat_in16,
	inl:	lba_pat_in32,
	outb:	lba_pat_out8,
	outw:	lba_pat_out16,
	outl:	lba_pat_out32
};



/*
** make range information from PDC available to PCI subsystem.
** We make the PDC call here in order to get the PCI bus range
** numbers. The rest will get forwarded in pcibios_fixup_bus().
** We don't have a struct pci_bus assigned to us yet.
*/
static void
lba_pat_resources(struct parisc_device *pa_dev, struct lba_device *lba_dev)
{
	unsigned long bytecnt;
	pdc_pat_cell_mod_maddr_block_t pa_pdc_cell;	/* PA_VIEW */
	pdc_pat_cell_mod_maddr_block_t io_pdc_cell;	/* IO_VIEW */
	long io_count;
	long status;	/* PDC return status */
	long pa_count;
	int i;

	/* return cell module (IO view) */
	status = pdc_pat_cell_module(&bytecnt, pa_dev->pcell_loc, pa_dev->mod_index,
				PA_VIEW, & pa_pdc_cell);
	pa_count = pa_pdc_cell.mod[1];

	status |= pdc_pat_cell_module(&bytecnt, pa_dev->pcell_loc, pa_dev->mod_index,
				IO_VIEW, &io_pdc_cell);
	io_count = io_pdc_cell.mod[1];

	/* We've already done this once for device discovery...*/
	if (status != PDC_OK) {
		panic("pdc_pat_cell_module() call failed for LBA!\n");
	}

	if (PAT_GET_ENTITY(pa_pdc_cell.mod_info) != PAT_ENTITY_LBA) {
		panic("pdc_pat_cell_module() entity returned != PAT_ENTITY_LBA!\n");
	}

	/*
	** Inspect the resources PAT tells us about
	*/
	for (i = 0; i < pa_count; i++) {
		struct {
			unsigned long type;
			unsigned long start;
			unsigned long end;	/* aka finish */
		} *p, *io;
		struct resource *r;

		p = (void *) &(pa_pdc_cell.mod[2+i*3]);
		io = (void *) &(io_pdc_cell.mod[2+i*3]);

		/* Convert the PAT range data to PCI "struct resource" */
		switch(p->type & 0xff) {
		case PAT_PBNUM:
			lba_dev->hba.bus_num.start = p->start;
			lba_dev->hba.bus_num.end   = p->end;
			break;
		case PAT_LMMIO:
			/* used to fix up pre-initialized MEM BARs */
			lba_dev->hba.lmmio_space_offset = p->start - io->start;

			r = &(lba_dev->hba.lmmio_space);
			r->name   = "LBA LMMIO";
			r->start  = p->start;
			r->end    = p->end;
			r->flags  = IORESOURCE_MEM;
			r->parent = r->sibling = r->child = NULL;
			break;
		case PAT_GMMIO:
			printk(KERN_WARNING MODULE_NAME
				" range[%d] : ignoring GMMIO (0x%lx)\n",
				i, p->start);
			lba_dev->gmmio_base = p->start;
			break;
		case PAT_NPIOP:
			printk(KERN_WARNING MODULE_NAME
				" range[%d] : ignoring NPIOP (0x%lx)\n",
				i, p->start);
			break;
		case PAT_PIOP:
			/*
			** Postable I/O port space is per PCI host adapter.
			*/

			/* save base of 64MB PIOP region */
			lba_dev->iop_base = p->start;

			r = &(lba_dev->hba.io_space);
			r->name   = "LBA I/O Port";
			r->start  = HBA_PORT_BASE(lba_dev->hba.hba_num);
			r->end    = r->start + HBA_PORT_SPACE_SIZE - 1;
			r->flags  = IORESOURCE_IO;
			r->parent = r->sibling = r->child = NULL;
			break;
		default:
			printk(KERN_WARNING MODULE_NAME
				" range[%d] : unknown pat range type (0x%lx)\n",
				i, p->type & 0xff);
			break;
		}
	}
}
#endif	/* __LP64__ */


static void
lba_legacy_resources(struct parisc_device *pa_dev, struct lba_device *lba_dev)
{
	struct resource *r;
	unsigned long rsize;
	int lba_num;

#ifdef __LP64__
	/*
	** Sign extend all BAR values on "legacy" platforms.
	** "Sprockets" PDC (Forte/Allegro) initializes everything
	** for "legacy" 32-bit OS (HPUX 10.20).
	** Upper 32-bits of 64-bit BAR will be zero too.
	*/
	lba_dev->hba.lmmio_space_offset = 0xffffffff00000000UL;
#else
	lba_dev->hba.lmmio_space_offset = 0UL;
#endif

	/*
	** With "legacy" firmware, the lowest byte of FW_SCRATCH
	** represents bus->secondary and the second byte represents
	** bus->subsidiary (i.e. highest PPB programmed by firmware).
	** PCI bus walk *should* end up with the same result.
	** FIXME: But we don't have sanity checks in PCI or LBA.
	*/
	lba_num = READ_REG32(pa_dev->hpa + LBA_FW_SCRATCH);
	r = &(lba_dev->hba.bus_num);
	r->name = "LBA PCI Busses";
	r->start = lba_num & 0xff;
	r->end = (lba_num>>8) & 0xff;

	/* Set up local PCI Bus resources - we don't really need
	** them for Legacy boxes but it's nice to see in /proc.
	*/
	r = &(lba_dev->hba.lmmio_space);
	r->name  = "LBA PCI LMMIO";
	r->flags = IORESOURCE_MEM;
	/* Ignore "Range Enable" bit in the BASE register */
	r->start = PCI_HOST_ADDR(HBA_DATA(lba_dev),
		((long) READ_REG32(pa_dev->hpa + LBA_LMMIO_BASE)) & ~1UL);
	rsize =  ~READ_REG32(pa_dev->hpa + LBA_LMMIO_MASK) + 1;

	/*
	** Each rope only gets part of the distributed range.
	** Adjust "window" for this rope
	*/
	rsize /= ROPES_PER_SBA;
	r->start += rsize * LBA_NUM(pa_dev->hpa);
	r->end = r->start + rsize - 1 ;

	/*
	** XXX FIXME - ignore LBA_ELMMIO_BASE for now
	** "Directed" ranges are used when the "distributed range" isn't
	** sufficient for all devices below a given LBA.  Typically devices
	** like graphics cards or X25 may need a directed range when the
	** bus has multiple slots (ie multiple devices) or the device
	** needs more than the typical 4 or 8MB a distributed range offers.
	**
	** The main reason for ignoring it now frigging complications.
	** Directed ranges may overlap (and have precedence) over
	** distributed ranges. Ie a distributed range assigned to a unused
	** rope may be used by a directed range on a different rope.
	** Support for graphics devices may require fixing this
	** since they may be assigned a directed range which overlaps
	** an existing (but unused portion of) distributed range.
	*/
	r = &(lba_dev->hba.elmmio_space);
	r->name  = "extra LBA PCI LMMIO";
	r->flags = IORESOURCE_MEM;
	r->start = READ_REG32(pa_dev->hpa + LBA_ELMMIO_BASE);
	r->end   = 0;

	/* check Range Enable bit */
	if (r->start & 1) {
		/* First baby step to getting Direct Ranges listed in /proc.
		** AFAIK, only Sprockets PDC will setup a directed Range.
		*/

		r->start &= ~1;
		r->end    = r->start;
		r->end   += ~READ_REG32(pa_dev->hpa + LBA_ELMMIO_MASK);
		printk(KERN_DEBUG "WARNING: Ignoring enabled ELMMIO BASE 0x%0lx  SIZE 0x%lx\n",
			r->start,
			r->end + 1);

	}

	r = &(lba_dev->hba.io_space);
	r->name  = "LBA PCI I/O Ports";
	r->flags = IORESOURCE_IO;
	r->start = READ_REG32(pa_dev->hpa + LBA_IOS_BASE) & ~1L;
	r->end   = r->start + (READ_REG32(pa_dev->hpa + LBA_IOS_MASK) ^ (HBA_PORT_SPACE_SIZE - 1));

	/* Virtualize the I/O Port space ranges */
	lba_num = HBA_PORT_BASE(lba_dev->hba.hba_num);
	r->start |= lba_num;
	r->end   |= lba_num;
}


/**************************************************************************
**
**   LBA initialization code (HW and SW)
**
**   o identify LBA chip itself
**   o initialize LBA chip modes (HardFail)
**   o FIXME: initialize DMA hints for reasonable defaults
**   o enable configuration functions
**   o call pci_register_ops() to discover devs (fixup/fixup_bus get invoked)
**
**************************************************************************/

static int __init
lba_hw_init(struct lba_device *d)
{
	u32 stat;
	u32 bus_reset;	/* PDC_PAT_BUG */

#if 0
	printk(KERN_DEBUG "LBA %lx  STAT_CTL %Lx  ERROR_CFG %Lx  STATUS %Lx DMA_CTL %Lx\n",
		d->hba.base_addr,
		READ_REG64(d->hba.base_addr + LBA_STAT_CTL),
		READ_REG64(d->hba.base_addr + LBA_ERROR_CONFIG),
		READ_REG64(d->hba.base_addr + LBA_ERROR_STATUS),
		READ_REG64(d->hba.base_addr + LBA_DMA_CTL) );
	printk(KERN_DEBUG "	ARB mask %Lx  pri %Lx  mode %Lx  mtlt %Lx\n",
		READ_REG64(d->hba.base_addr + LBA_ARB_MASK),
		READ_REG64(d->hba.base_addr + LBA_ARB_PRI),
		READ_REG64(d->hba.base_addr + LBA_ARB_MODE),
		READ_REG64(d->hba.base_addr + LBA_ARB_MTLT) );
	printk(KERN_DEBUG "	HINT cfg 0x%Lx\n",
		READ_REG64(d->hba.base_addr + LBA_HINT_CFG));
	printk(KERN_DEBUG "	HINT reg ");
	{ int i;
	for (i=LBA_HINT_BASE; i< (14*8 + LBA_HINT_BASE); i+=8)
		printk(" %Lx", READ_REG64(d->hba.base_addr + i));
	}
	printk("\n");
#endif	/* DEBUG_LBA_PAT */

#ifdef __LP64__
#warning FIXME add support for PDC_PAT_IO "Get slot status" - OLAR support
#endif

	/* PDC_PAT_BUG: exhibited in rev 40.48  on L2000 */
	bus_reset = READ_REG32(d->hba.base_addr + LBA_STAT_CTL + 4) & 1;
	if (bus_reset) {
		printk(KERN_DEBUG "NOTICE: PCI bus reset still asserted! (clearing)\n");
	}

	stat = READ_REG32(d->hba.base_addr + LBA_ERROR_CONFIG);
	if (stat & LBA_SMART_MODE) {
		printk(KERN_DEBUG "NOTICE: LBA in SMART mode! (cleared)\n");
		stat &= ~LBA_SMART_MODE;
		WRITE_REG32(stat, d->hba.base_addr + LBA_ERROR_CONFIG);
	}

	/* Set HF mode as the default (vs. -1 mode). */
        stat = READ_REG32(d->hba.base_addr + LBA_STAT_CTL);
	WRITE_REG32(stat | HF_ENABLE, d->hba.base_addr + LBA_STAT_CTL);

	/*
	** Writing a zero to STAT_CTL.rf (bit 0) will clear reset signal
	** if it's not already set. If we just cleared the PCI Bus Reset
	** signal, wait a bit for the PCI devices to recover and setup.
	*/
	if (bus_reset)
		mdelay(pci_post_reset_delay);

	if (0 == READ_REG32(d->hba.base_addr + LBA_ARB_MASK)) {
		/*
		** PDC_PAT_BUG: PDC rev 40.48 on L2000.
		** B2000/C3600/J6000 also have this problem?
		** 
		** Elroys with hot pluggable slots don't get configured
		** correctly if the slot is empty.  ARB_MASK is set to 0
		** and we can't master transactions on the bus if it's
		** not at least one. 0x3 enables elroy and first slot.
		*/
		printk(KERN_DEBUG "NOTICE: Enabling PCI Arbitration\n");
		WRITE_REG32(0x3, d->hba.base_addr + LBA_ARB_MASK);
	}

	/*
	** FIXME: Hint registers are programmed with default hint
	** values by firmware. Hints should be sane even if we
	** can't reprogram them the way drivers want.
	*/
	return 0;
}



static void __init
lba_common_init(struct lba_device *lba_dev)
{
	pci_bios = &lba_bios_ops;
	pcibios_register_hba(HBA_DATA(lba_dev));
	lba_dev->lba_lock = SPIN_LOCK_UNLOCKED;	

	/*
	** Set flags which depend on hw_rev
	*/
	if (!LBA_TR4PLUS(lba_dev)) {
		lba_dev->flags |= LBA_FLAG_NO_DMA_DURING_CFG;
	}
}



/*
** Determine if lba should claim this chip (return 0) or not (return 1).
** If so, initialize the chip and tell other partners in crime they
** have work to do.
*/
static int __init
lba_driver_callback(struct parisc_device *dev)
{
	struct lba_device *lba_dev;
	struct pci_bus *lba_bus;
	u32 func_class;
	void *tmp_obj;
	char *version;

	/* Read HW Rev First */
	func_class = READ_REG32(dev->hpa + LBA_FCLASS);
	func_class &= 0xf;

	switch (func_class) {
	case 0:	version = "TR1.0"; break;
	case 1:	version = "TR2.0"; break;
	case 2:	version = "TR2.1"; break;
	case 3:	version = "TR2.2"; break;
	case 4:	version = "TR3.0"; break;
	case 5:	version = "TR4.0"; break;
	default: version = "TR4+";
	}

	printk(KERN_INFO "%s version %s (0x%x) found at 0x%lx\n",
		MODULE_NAME, version, func_class & 0xf, dev->hpa);

	/* Just in case we find some prototypes... */
	if (func_class < 2) {
		printk(KERN_WARNING "Can't support LBA older than TR2.1 "
			"- continuing under adversity.\n");
	}

	/*
	** Tell I/O SAPIC driver we have a IRQ handler/region.
	*/
	tmp_obj = iosapic_register(dev->hpa + LBA_IOSAPIC_BASE);

	/* NOTE: PCI devices (e.g. 103c:1005 graphics card) which don't
	**	have an IRT entry will get NULL back from iosapic code.
	*/
	
	lba_dev = kmalloc(sizeof(struct lba_device), GFP_KERNEL);
	if (NULL == lba_dev)
	{
		printk(KERN_ERR "lba_init_chip - couldn't alloc lba_device\n");
		return(1);
	}

	memset(lba_dev, 0, sizeof(struct lba_device));


	/* ---------- First : initialize data we already have --------- */

	/*
	** Need hw_rev to adjust configuration space behavior.
	** LBA_TR4PLUS macro uses hw_rev field.
	*/
	lba_dev->hw_rev = func_class;

	lba_dev->hba.base_addr = dev->hpa;  /* faster access */
	lba_dev->hba.dev = dev;
	lba_dev->iosapic_obj = tmp_obj;  /* save interrupt handle */
	lba_dev->hba.iommu = sba_get_iommu(dev);  /* get iommu data */

	/* ------------ Second : initialize common stuff ---------- */
	lba_common_init(lba_dev);

	if (lba_hw_init(lba_dev))
		return(1);

	/* ---------- Third : setup I/O Port and MMIO resources  --------- */

#ifdef __LP64__
	if (is_pdc_pat()) {
		/* PDC PAT firmware uses PIOP region of GMMIO space. */
		pci_port = &lba_pat_port_ops;

		/* Go ask PDC PAT what resources this LBA has */
		lba_pat_resources(dev, lba_dev);
	} else
#endif
	{
		/* Sprockets PDC uses NPIOP region */
		pci_port = &lba_astro_port_ops;

		/* Poke the chip a bit for /proc output */
		lba_legacy_resources(dev, lba_dev);
	}

	/* 
	** Tell PCI support another PCI bus was found.
	** Walks PCI bus for us too.
	*/
	lba_bus = lba_dev->hba.hba_bus =
		pci_scan_bus(lba_dev->hba.bus_num.start, &lba_cfg_ops, (void *) lba_dev);

#ifdef __LP64__
	if (is_pdc_pat()) {
		/* assign resources to un-initialized devices */
		DBG_PAT("LBA pcibios_assign_unassigned_resources()\n");
		pcibios_assign_unassigned_resources(lba_bus);

#ifdef DEBUG_LBA_PAT
		DBG_PAT("\nLBA PIOP resource tree\n");
		lba_dump_res(&lba_dev->hba.io_space, 2);
		DBG_PAT("\nLBA LMMIO resource tree\n");
		lba_dump_res(&lba_dev->hba.lmmio_space, 2);
#endif
	}
#endif

	/*
	** Once PCI register ops has walked the bus, access to config
	** space is restricted. Avoids master aborts on config cycles.
	** Early LBA revs go fatal on *any* master abort.
	*/
	if (!LBA_TR4PLUS(lba_dev)) {
		lba_dev->flags |= LBA_FLAG_SKIP_PROBE;
	}

	/* Whew! Finally done! Tell services we got this one covered. */
	return 0;
}

static struct parisc_device_id lba_tbl[] = {
	{ HPHW_BRIDGE, HVERSION_REV_ANY_ID, 0x782, 0xa },
	{ 0, }
};

static struct parisc_driver lba_driver = {
	name:		MODULE_NAME,
	id_table:	lba_tbl,
	probe:		lba_driver_callback
};

/*
** One time initialization to let the world know the LBA was found.
** Must be called exactly once before pci_init().
*/
void __init lba_init(void)
{
	register_parisc_driver(&lba_driver);
}

/*
** Initialize the IBASE/IMASK registers for LBA (Elroy).
** Only called from sba_iommu.c in order to route ranges (MMIO vs DMA).
** sba_iommu is responsible for locking (none needed at init time).
*/
void
lba_set_iregs(struct parisc_device *lba, u32 ibase, u32 imask)
{
	unsigned long base_addr = lba->hpa;

	imask <<= 2;	/* adjust for hints - 2 more bits */

	ASSERT((ibase & 0x003fffff) == 0);
	ASSERT((imask & 0x003fffff) == 0);
	
	DBG("%s() ibase 0x%x imask 0x%x\n", __FUNCTION__, ibase, imask);
	WRITE_REG32( imask, base_addr + LBA_IMASK);
	WRITE_REG32( ibase, base_addr + LBA_IBASE);
}

