/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2003 Silicon Graphics, Inc.  All Rights Reserved.
 */


/* This is the top level IOC4 device driver.  It does very little, farming
 * out actual tasks to the various slave IOC4 drivers (serial, keyboard/mouse,
 * and real-time interrupt).
 */

#include <linux/config.h>
#include <asm/sn/types.h>
#include <asm/sn/sgi.h>
#include <asm/sn/invent.h>
#include <asm/sn/iograph.h>
#include <asm/atomic.h>
#include <asm/sn/pci/pci_defs.h>
#include <asm/sn/pci/pciio.h>
#include <linux/pci.h>
#include <asm/sn/ioc4.h>
#include <asm/sn/pci/pci_bus_cvlink.h>

/* #define DEBUG_INTERRUPTS */
#define SUPPORT_ATOMICS

#ifdef SUPPORT_ATOMICS

/*
 * support routines for local atomic operations.
 */

static spinlock_t local_lock;

static inline unsigned int
atomicSetInt(atomic_t *a, unsigned int b)
{
	unsigned long s;
	unsigned int ret, new;

	spin_lock_irqsave(&local_lock, s);
	new = ret = atomic_read(a);
	new |= b;
	atomic_set(a, new);
	spin_unlock_irqrestore(&local_lock, s);

	return ret;
}

static unsigned int
atomicClearInt(atomic_t *a, unsigned int b)
{
	unsigned long s;
	unsigned int ret, new;

	spin_lock_irqsave(&local_lock, s);
	new = ret = atomic_read(a);
	new &= ~b;
	atomic_set(a, new);
	spin_unlock_irqrestore(&local_lock, s);

	return ret;
}

#else

#define atomicAddInt(a,b)	*(a) += ((unsigned int)(b))

static inline unsigned int
atomicSetInt(unsigned int *a, unsigned int b)
{
	unsigned int ret = *a;

	*a |= b;
	return ret;
}

#define atomicSetUint64(a,b)	*(a) |= ((unsigned long long )(b))

static inline unsigned int
atomicClearInt(unsigned int *a, unsigned int b)
{
	unsigned int ret = *a;

	*a &= ~b;
	return ret;
}

#define atomicClearUint64(a,b)	*(a) &= ~((unsigned long long)(b))
#endif	/* SUPPORT_ATOMICS */


/* pci device struct */
static const struct pci_device_id __devinitdata ioc4_s_id_table[] =
{
        { IOC4_VENDOR_ID_NUM, IOC4_DEVICE_ID_NUM, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
        {              0,           0,          0,          0, 0, 0, 0 }
};

int __devinit ioc4_attach(struct pci_dev *, const struct pci_device_id *);

struct pci_driver ioc4_s_driver =
{
	name : "IOC4 Serial",
	id_table: ioc4_s_id_table,
	probe: ioc4_attach,
};

int __init
ioc4_serial_detect(void)
{
	int rc;

	rc = pci_register_driver(&ioc4_s_driver);
	return 0;
}
module_init(ioc4_serial_detect);


/*
 * Some external functions we still need.
 */
extern int		ioc4_serial_attach(vertex_hdl_t conn, void *mem);
extern cpuid_t		cpuvertex_to_cpuid(vertex_hdl_t vhdl);


/*
 * per-IOC4 data structure
 */
typedef struct ioc4_soft_s {
    vertex_hdl_t	    is_ioc4_vhdl;
    vertex_hdl_t            is_conn_vhdl;

    struct pci_dev	   *is_pci_dev;
    ioc4_mem_t		   *is_ioc4_mem;

    /* Each interrupt type has an entry in the array */
    struct ioc4_intr_type {

        /*
         * Each in-use entry in this array contains at least
         * one nonzero bit in sd_bits; no two entries in this
         * array have overlapping sd_bits values.
         */
#define MAX_IOC4_INTR_ENTS	(8 * sizeof(ioc4reg_t))
        struct ioc4_intr_info {
	    ioc4reg_t		sd_bits;
	    ioc4_intr_func_f   *sd_intr;
	    intr_arg_t		sd_info;
	    vertex_hdl_t	sd_vhdl;
	    struct ioc4_soft_s *sd_soft;
        } is_intr_info[MAX_IOC4_INTR_ENTS];

        /* Number of entries active in the above array */
        atomic_t	 is_num_intrs;
        atomic_t	 is_intr_bits_busy;	/* Bits assigned */
        atomic_t         is_intr_ents_free;	/* Free active entries mask*/
    } is_intr_type[ioc4_num_intr_types];

    /* is_ir_lock must be held while
     * modifying sio_ie values, so
     * we can be sure that sio_ie is
     * not changing when we read it
     * along with sio_ir.
     */
    spinlock_t		    is_ir_lock;		/* SIO_IE[SC] mod lock */
} ioc4_soft_t;

#define ioc4_soft_set(v,i)	hwgraph_fastinfo_set((v), (arbitrary_info_t)(i))
#define ioc4_soft_get(v)	((ioc4_soft_t *)hwgraph_fastinfo_get(v))


/* =====================================================================
 *    Function Table of Contents
 */


/* The IOC4 hardware provides no atomic way to determine if interrupts
 * are pending since two reads are required to do so.  The handler must
 * read the SIO_IR and the SIO_IES, and take the logical and of the
 * two.  When this value is zero, all interrupts have been serviced and
 * the handler may return.
 *
 * This has the unfortunate "hole" that, if some other CPU or
 * some other thread or some higher level interrupt manages to
 * modify SIO_IE between our reads of SIO_IR and SIO_IE, we may
 * think we have observed SIO_IR&SIO_IE==0 when in fact this
 * condition never really occurred.
 *
 * To solve this, we use a simple spinlock that must be held
 * whenever modifying SIO_IE; holding this lock while observing
 * both SIO_IR and SIO_IE guarantees that we do not falsely
 * conclude that no enabled interrupts are pending.
 */

void
ioc4_write_ireg(void *ioc4_soft, ioc4reg_t val, int which, ioc4_intr_type_t type)
{
    ioc4_mem_t		   *mem = ((ioc4_soft_t *) ioc4_soft)->is_ioc4_mem;
    spinlock_t		   *lp = &((ioc4_soft_t *) ioc4_soft)->is_ir_lock;
    unsigned long	    s;


    spin_lock_irqsave(lp, s);

    switch (type) {
      case ioc4_sio_intr_type:
        switch (which) {
          case IOC4_W_IES:
	    mem->sio_ies_ro = val;
	    break;

          case IOC4_W_IEC:
	    mem->sio_iec_ro = val;
	    break;
        }
	break;

      case ioc4_other_intr_type:
        switch (which) {
          case IOC4_W_IES:
	    mem->other_ies_ro = val;
	    break;

          case IOC4_W_IEC:
	    mem->other_iec_ro = val;
	    break;
        }
	break;

      case ioc4_num_intr_types:
	break;
    }
    spin_unlock_irqrestore(lp, s);
}


static inline ioc4reg_t
ioc4_pending_intrs(ioc4_soft_t * ioc4_soft, ioc4_intr_type_t type)
{
    ioc4_mem_t	*mem = ioc4_soft->is_ioc4_mem;
    spinlock_t	*lp = &ioc4_soft->is_ir_lock;
    unsigned long	    s;
    ioc4reg_t	intrs = (ioc4reg_t)0;

    ASSERT((type == ioc4_sio_intr_type) || (type == ioc4_other_intr_type));

    spin_lock_irqsave(lp, s);

    switch (type) {
      case ioc4_sio_intr_type:
        intrs = mem->sio_ir & mem->sio_ies_ro;
        break;

      case ioc4_other_intr_type:
        intrs = mem->other_ir & mem->other_ies_ro;

        /* Don't process any ATA interrupte, leave them for the ATA driver */
        intrs &= ~(IOC4_OTHER_IR_ATA_INT | IOC4_OTHER_IR_ATA_MEMERR); 
        break;

      case ioc4_num_intr_types:
	break;
    }

    spin_unlock_irqrestore(lp, s);
    return intrs;
}


int __devinit
ioc4_attach(struct pci_dev *pci_handle, const struct pci_device_id *pci_id)
{
    ioc4_mem_t		   *mem;
/*REFERENCED*/
    graph_error_t	    rc;
    vertex_hdl_t	    ioc4_vhdl;
    ioc4_soft_t		   *soft;
    vertex_hdl_t	    conn_vhdl = PCIDEV_VERTEX(pci_handle);
    int                     tmp;
    extern void ioc4_ss_connect_interrupt(int, void *, void *);
    extern void ioc4_intr(int, void *, struct pt_regs *);

    if ( pci_enable_device(pci_handle) ) {
	    printk("ioc4_attach: Failed to enable device with pci_dev 0x%p... returning\n", (void *)pci_handle);
	    return(-1);
    }

    pci_set_master(pci_handle);
    snia_pciio_endian_set(pci_handle, PCIDMA_ENDIAN_LITTLE, PCIDMA_ENDIAN_BIG);

    /*
     * Get PIO mappings through our "primary"
     * connection point to the IOC4's CFG and
     * MEM spaces.
     */

    /*
     * Map in the ioc4 memory - we'll do config accesses thru the pci_????() interfaces.
     */

    mem = (ioc4_mem_t *)pci_resource_start(pci_handle, 0);
    if ( !mem ) {
	printk(KERN_ALERT "%p/" EDGE_LBL_IOC4
		": unable to get PIO mapping for my MEM space\n", (void *)pci_handle);
	return -1;
    }

    if ( !request_region((unsigned long)mem, sizeof(*mem), "sioc4_mem")) {
	printk(KERN_ALERT
		"%p/" EDGE_LBL_IOC4
		": unable to get request region for my MEM space\n",
		(void *)pci_handle);
	return -1;
    }		

    /*
     * Create the "ioc4" vertex which hangs off of
     * the connect points.
     * This code is slightly paranoid.
     */
    rc = hwgraph_path_add(conn_vhdl, EDGE_LBL_IOC4, &ioc4_vhdl);
    ASSERT(rc == GRAPH_SUCCESS);

    /*
     * Allocate the soft structure, fill it in a bit,
     * and attach it to the ioc4 vertex.
     */
    NEW(soft);

    spin_lock_init(&soft->is_ir_lock);
    soft->is_ioc4_vhdl = ioc4_vhdl;
    soft->is_conn_vhdl = conn_vhdl;
    soft->is_ioc4_mem = mem;
    soft->is_pci_dev = pci_handle;

    ioc4_soft_set(ioc4_vhdl, soft);

    /* Init the IOC4 */

    /* SN boot PROMs allocate the PCI
     * space and set up the pci_addr fields.
     * Other systems need to set the base address.
     * This is handled automatically if the PCI infrastructure
     * is used.
     *
     * No need to set the latency timer since the PCI
     * infrastructure sets it to 1 us.
     */

    pci_read_config_dword(pci_handle, IOC4_PCI_SCR, &tmp);

    pci_write_config_dword(pci_handle, IOC4_PCI_SCR, 
	     tmp | PCI_CMD_BUS_MASTER | PCI_CMD_MEM_SPACE |
	     PCI_CMD_PAR_ERR_RESP | PCI_CMD_SERR_ENABLE);

    PCI_OUTW(&mem->sio_cr, (0xf << IOC4_SIO_CR_CMD_PULSE_SHIFT));

    /* Enable serial port mode select generic PIO pins as outputs */
    PCI_OUTW(&mem->gpcr_s, IOC4_GPCR_UART0_MODESEL | IOC4_GPCR_UART1_MODESEL);

    /* Clear and disable all interrupts */
    IOC4_WRITE_IEC(soft, ~0, ioc4_sio_intr_type);
    PCI_OUTW(&mem->sio_ir, ~0);

    IOC4_WRITE_IEC(soft, ~0, ioc4_other_intr_type);
    PCI_OUTW(&mem->other_ir, ~0);

    /*
     * Alloc the IOC4 intr before attaching the subdevs, so the
     * cpu handling the IOC4 intr is known (for setmustrun on
     * the ioc4 ithreads).
     */

	/* attach interrupt handler */

	ioc4_ss_connect_interrupt(pci_handle->irq, (void *)ioc4_intr, (void *)soft);

    /* =============================================================
     *				  Attach Sub-devices
     *
     * NB: As subdevs start calling pciio_driver_register(),
     * we can stop explicitly calling subdev drivers.
     *
     * The drivers attached here have not been converted
     * to stand on their own.  However, they *do* know
     * to call ioc4_subdev_enabled() to decide whether
     * to actually attach themselves.
     *
     * It would be nice if we could convert these
     * few remaining drivers over so they would
     * register as proper PCI device drivers ...
     */

    ioc4_serial_attach(conn_vhdl, (void *)soft->is_ioc4_mem);	/* DMA serial ports */

    /* Normally we'd return 0 - but we need to get the ide driver init'd too. 
     * Returning an error will keep the IOC4 on the pci list */
    return -1;
}


/*
 * ioc4_intr_connect:
 * Arrange for interrupts for a sub-device
 * to be delivered to the right bit of
 * code with the right parameter.
 *
 * XXX- returning an error instead of panicing
 * might be a good idea (think bugs in loadable
 * ioc4 sub-devices).
 */



void
ioc4_intr_connect(vertex_hdl_t conn_vhdl,
		  ioc4_intr_type_t type,
		  ioc4reg_t intrbits,
		  ioc4_intr_func_f *intr,
		  intr_arg_t info,
		  vertex_hdl_t owner_vhdl,
		  vertex_hdl_t intr_dev_vhdl)
{
    graph_error_t	    rc;
    vertex_hdl_t	    ioc4_vhdl;
    ioc4_soft_t		   *soft;
    ioc4reg_t		    old, bits;
    int			    i;

    ASSERT((type == ioc4_sio_intr_type) || (type == ioc4_other_intr_type));

    rc = hwgraph_traverse(conn_vhdl, EDGE_LBL_IOC4, &ioc4_vhdl);
    if (rc != GRAPH_SUCCESS) {
	printk(KERN_ALERT "ioc4_intr_connect(%p): ioc4_attach not yet called", (void *)owner_vhdl);
	return;
    }

    soft = ioc4_soft_get(ioc4_vhdl);
    ASSERT(soft != NULL);

    /*
     * Try to allocate a slot in the array
     * that has been marked free; if there
     * are none, extend the high water mark.
     */
    while (1) {
	bits = atomic_read(&soft->is_intr_type[type].is_intr_ents_free);
	if (bits == 0) {
	    i = atomic_inc(&soft->is_intr_type[type].is_num_intrs) - 1;
	    ASSERT(i < MAX_IOC4_INTR_ENTS || (printk("i %d\n", i), 0));
	    break;
	}
	bits &= ~(bits - 1);	/* keep only the ls bit */
	old = atomicClearInt(&soft->is_intr_type[type].is_intr_ents_free, bits);
	if (bits & old) {
		ioc4reg_t		shf;

		i = 31;
		if ((shf = (bits >> 16)))
		    bits = shf;
		else
		    i -= 16;
		if ((shf = (bits >> 8)))
		    bits = shf;
		else
		    i -= 8;
		if ((shf = (bits >> 4)))
		    bits = shf;
		else
		    i -= 4;
		if ((shf = (bits >> 2)))
		    bits = shf;
		else
		    i -= 2;
		if ((shf = (bits >> 1)))
		    bits = shf;
		else
		    i -= 1;
	    ASSERT(i < MAX_IOC4_INTR_ENTS || (printk("i %d\n", i), 0));
	    break;
	}
    }

    soft->is_intr_type[type].is_intr_info[i].sd_bits = intrbits;
    soft->is_intr_type[type].is_intr_info[i].sd_intr = intr;
    soft->is_intr_type[type].is_intr_info[i].sd_info = info;
    soft->is_intr_type[type].is_intr_info[i].sd_vhdl = owner_vhdl;
    soft->is_intr_type[type].is_intr_info[i].sd_soft = soft;

    /* Make sure there are no bitmask overlaps */
    {
	ioc4reg_t		old;

	old = atomicSetInt(&soft->is_intr_type[type].is_intr_bits_busy, intrbits);
	if (old & intrbits) {
	    printk("%p: trying to share ioc4 intr bits 0x%X\n",
		    (void *)owner_vhdl, old & intrbits);

#if DEBUG && IOC4_DEBUG
	    {
		int			x;

		for (x = 0; x < i; x++)
		    if (intrbits & soft->is_intr_type[type].is_intr_info[x].sd_bits) {
			printk("%p: ioc4 intr bits 0x%X already call "
				"0x%X(0x%X, ...)\n",
				(void *)soft->is_intr_type[type].is_intr_info[x].sd_vhdl,
				soft->is_intr_type[type].is_intr_info[i].sd_bits,
				soft->is_intr_type[type].is_intr_info[i].sd_intr,
				soft->is_intr_type[type].is_intr_info[i].sd_info);
		    }
	    }
#endif
	    panic("ioc4_intr_connect: no IOC4 interrupt source sharing allowed");
	}
    }
}

/*
 * ioc4_intr_disconnect:
 * Turn off interrupt request service for a
 * specific service function and argument.
 * Scans the array for connections to the specified
 * function with the specified info and owner; turns off
 * the bits specified in intrbits.  If this results in
 * an empty entry, logs it in the free entry map.
 */
void
ioc4_intr_disconnect(vertex_hdl_t conn_vhdl,
		     ioc4_intr_type_t type,
		     ioc4reg_t intrbits,
		     ioc4_intr_func_f *intr,
		     intr_arg_t info,
		     vertex_hdl_t owner_vhdl)
{
    graph_error_t	    rc;
    vertex_hdl_t	    ioc4_vhdl;
    ioc4_soft_t		   *soft;
    ioc4reg_t		    bits;
    int			    i, num_intrs;

    ASSERT((type == ioc4_sio_intr_type) || (type == ioc4_other_intr_type));

    rc = hwgraph_traverse(conn_vhdl, EDGE_LBL_IOC4, &ioc4_vhdl);
    if (rc != GRAPH_SUCCESS) {
	printk(KERN_ALERT "%p: ioc4_intr_disconnect: ioc4_attach not yet called", (void *)owner_vhdl);
	return;
    }

    soft = ioc4_soft_get(ioc4_vhdl);
    ASSERT(soft != NULL);

    num_intrs = (int)atomic_read(&soft->is_intr_type[type].is_num_intrs);
    for (i = 0; i < num_intrs; ++i) {
	if ((soft->is_intr_type[type].is_intr_info[i].sd_intr == intr) &&
	    	(soft->is_intr_type[type].is_intr_info[i].sd_info == info) &&
	    	(soft->is_intr_type[type].is_intr_info[i].sd_vhdl == owner_vhdl) &&
	    	(bits = soft->is_intr_type[type].is_intr_info[i].sd_bits & intrbits)) {
	    soft->is_intr_type[type].is_intr_info[i].sd_bits &= ~bits;
	    atomicClearInt(&soft->is_intr_type[type].is_intr_bits_busy, bits);
	    if (!(soft->is_intr_type[type].is_intr_info[i].sd_bits)) {
		soft->is_intr_type[type].is_intr_info[i].sd_intr = NULL;
		soft->is_intr_type[type].is_intr_info[i].sd_info = NULL;
		soft->is_intr_type[type].is_intr_info[i].sd_vhdl = GRAPH_VERTEX_NONE;
		atomicSetInt(&soft->is_intr_type[type].is_intr_ents_free, 1 << i);
	    }
	}
    }
}

/* Top level IOC4 interrupt handler.  Farms out the interrupt to
 * the various IOC4 device drivers.
 */

void
ioc4_intr(int irq, void *arg, struct pt_regs *regs)
{
    ioc4_soft_t		   *soft;
    ioc4reg_t		    this_ir;
    ioc4reg_t		    this_mir;
    int			    x, num_intrs = 0;
    ioc4_intr_type_t        t;

    soft = (ioc4_soft_t *)arg;

    if (!soft)
	return;			/* Polled but no console ioc4 registered */

    for (t = ioc4_first_intr_type; t < ioc4_num_intr_types; t++) {
        num_intrs = (int)atomic_read(&soft->is_intr_type[t].is_num_intrs);

        this_mir = this_ir = ioc4_pending_intrs(soft, t);
#ifdef DEBUG_INTERRUPTS
	printk("%s : %d : this_mir 0x%x num_intrs %d\n", __FUNCTION__, __LINE__, this_mir, num_intrs);
#endif

        /* Farm out the interrupt to the various drivers depending on
         * which interrupt bits are set.
	 */
        for (x = 0; x < num_intrs; x++) {
		struct ioc4_intr_info  *ii = &soft->is_intr_type[t].is_intr_info[x];
		if ((this_mir = this_ir & ii->sd_bits)) {
			/* Disable owned interrupts, and call the interrupt handler */
			IOC4_WRITE_IEC(soft, ii->sd_bits, t);
			ii->sd_intr(ii->sd_info, this_mir);
			this_ir &= ~this_mir;
		}
	}

	if (this_ir)
		printk(KERN_ALERT "unknown IOC4 %s interrupt 0x%x, sio_ir = 0x%x, sio_ies = 0x%x, other_ir = 0x%x, other_ies = 0x%x\n",
				(t == ioc4_sio_intr_type) ? "sio" : "other",
				this_ir,
				soft->is_ioc4_mem->sio_ir,
				soft->is_ioc4_mem->sio_ies_ro,
				soft->is_ioc4_mem->other_ir,
				soft->is_ioc4_mem->other_ies_ro);
    }
#ifdef DEBUG_INTERRUPTS
    {
	ioc4_mem_t  *mem = soft->is_ioc4_mem;
	spinlock_t  *lp = &soft->is_ir_lock;
	unsigned long           s;

	spin_lock_irqsave(lp, s);
	printk("%s : %d : sio_ir 0x%x sio_ies_ro 0x%x other_ir 0x%x other_ies_ro 0x%x mask 0x%x\n",
			__FUNCTION__, __LINE__,
			mem->sio_ir,
			mem->sio_ies_ro,
			mem->other_ir,
			mem->other_ies_ro,
			IOC4_OTHER_IR_ATA_INT | IOC4_OTHER_IR_ATA_MEMERR);

	spin_unlock_irqrestore(lp, s);
    }
#endif
}
