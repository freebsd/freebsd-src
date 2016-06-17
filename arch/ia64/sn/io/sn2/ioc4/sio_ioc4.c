/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2003 Silicon Graphics, Inc.  All Rights Reserved.
 */

/*
 * This is a lower level module for the modular serial I/O driver. This
 * module implements all hardware dependent functions for doing serial
 * I/O on the IOC4 serial ports.
 */

#include <linux/config.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <asm/sn/types.h>
#include <asm/sn/sgi.h>
#include <asm/sn/invent.h>
#include <asm/sn/driver.h>
#include <asm/sn/iograph.h>
#include <asm/param.h>
#include <asm/atomic.h>
#include <asm/delay.h>
#include <asm/semaphore.h>
#include <asm/sn/pio.h>
#include <asm/sn/xtalk/xwidget.h>
#include <asm/sn/io.h>
#include <asm/sn/pci/pci_defs.h>
#include <asm/sn/pci/pciio.h>
#include <asm/sn/ioc4.h>
#include <asm/sn/serialio.h>
#include <asm/sn/uart16550.h>

/* #define IOC4_SIO_DEBUG */
/* define USE_64BIT_DMA */

#define PENDING(port) (PCI_INW(&(port)->ip_ioc4->sio_ir) & port->ip_ienb)

/* Default to 4k buffers */
#ifdef IOC4_1K_BUFFERS
#define RING_BUF_SIZE 1024
#define IOC4_BUF_SIZE_BIT 0
#define PROD_CONS_MASK IOC4_PROD_CONS_PTR_1K
#else
#define RING_BUF_SIZE 4096
#define IOC4_BUF_SIZE_BIT IOC4_SBBR_L_SIZE
#define PROD_CONS_MASK IOC4_PROD_CONS_PTR_4K
#endif

#define TOTAL_RING_BUF_SIZE (RING_BUF_SIZE * 4)

#if PAGE_SIZE < TOTAL_RING_BUF_SIZE
#include <sys/pfdat.h>
#endif


#ifdef DPRINTF
#define dprintf(x) printk x
#else
#define dprintf(x)
#endif

#define	NEWA(ptr,n)	(ptr = snia_kmem_zalloc((n)*sizeof (*(ptr))))

#define	contig_memalloc(a,b,c)	kmem_zalloc(PAGE_SIZE * (a))
#define sio_port_islocked(a)	0	// FIXME: ?????

#define KM_PHYSCONTIG   0x0008
#define VM_DIRECT       KM_PHYSCONTIG
#define VM_PHYSCONTIG   KM_PHYSCONTIG

#ifdef DEBUG
#define PROGRESS()	printk("%s : %d\n", __FUNCTION__, __LINE__)
#define NOT_PROGRESS()	printk("%s : %d - Error\n", __FUNCTION__, __LINE__)
#else
#define PROGRESS()	;
#define NOT_PROGRESS()	;
#endif

static __inline__ void *
kvpalloc(size_t size, int flags, int colour)
{
        if (flags & (VM_DIRECT|VM_PHYSCONTIG)) {
                int order = 0;
                while ((PAGE_SIZE << order) < (size << PAGE_SHIFT))
                        order++;
                return (void *) __get_free_pages(GFP_KERNEL, order);
        } else
                return vmalloc(size << PAGE_SHIFT);
}

/* Local port info for the IOC4 serial ports.  This contains as its
 * first member the global sio port private data.
 */
typedef struct ioc4port {
    sioport_t		ip_sioport;	/* Must be first struct entry! */

    vertex_hdl_t	ip_conn_vhdl;	/* vhdl to use for pciio requests */
    vertex_hdl_t	ip_port_vhdl;	/* vhdl for the serial port */

    /* Base piomap addr of the ioc4 board this port is on
     * and associated serial map;  serial map includes uart registers.
     */
    ioc4_mem_t	       *ip_ioc4;
    ioc4_sregs_t       *ip_serial;
    ioc4_uart_t        *ip_uart;

    /* Ring buffer page for this port */
    caddr_t		ip_ring_buf_k0;	/* Ring buffer location in K0 space */

    /* Rings for this port */
    struct ring	       *ip_inring;
    struct ring	       *ip_outring;

    /* Hook to port specific values for this port */
    struct hooks       *ip_hooks;

    int			ip_flags;

    /* Cache of DCD/CTS bits last received */
    char                ip_modem_bits;

    /* Various rx/tx parameters */
    int                 ip_baud;
    int                 ip_tx_lowat;
    int                 ip_rx_timeout;

    /* Copy of notification bits */
    int                 ip_notify;

    /* Shadow copies of various registers so we don't need to PIO
     * read them constantly
     */
    ioc4reg_t           ip_ienb;          /* Enabled interrupts */

    ioc4reg_t           ip_sscr;

    ioc4reg_t           ip_tx_prod;
    ioc4reg_t           ip_rx_cons;

    /* Back pointer to ioc4 soft area */
    void               *ip_ioc4_soft;
} ioc4port_t;

#if DEBUG
#define     MAXSAVEPORT 256
static int         next_saveport = 0;
static ioc4port_t *saveport[MAXSAVEPORT];
#endif

/* TX low water mark.  We need to notify the driver whenever TX is getting
 * close to empty so it can refill the TX buffer and keep things going.
 * Let's assume that if we interrupt 1 ms before the TX goes idle, we'll
 * have no trouble getting in more chars in time (I certainly hope so).
 */
#define TX_LOWAT_LATENCY      1000
#define TX_LOWAT_HZ          (1000000 / TX_LOWAT_LATENCY)
#define TX_LOWAT_CHARS(baud) (baud / 10 / TX_LOWAT_HZ)

/* Flags per port */
#define INPUT_HIGH	0x01
#define DCD_ON		0x02
#define LOWAT_WRITTEN	0x04
#define READ_ABORTED	0x08
#define TX_DISABLED	0x10

/* Get local port type from global sio port type */
#define LPORT(port) ((ioc4port_t *) (port))

/* Get global port from local port type */
#define GPORT(port) ((sioport_t *) (port))

/* Since each port has different register offsets and bitmasks
 * for everything, we'll store those that we need in tables so we
 * don't have to be constantly checking the port we are dealing with.
 */
struct hooks {
    ioc4reg_t intr_delta_dcd;
    ioc4reg_t intr_delta_cts;
    ioc4reg_t intr_tx_mt;
    ioc4reg_t intr_rx_timer;
    ioc4reg_t intr_rx_high;
    ioc4reg_t intr_tx_explicit;
    ioc4reg_t intr_dma_error;
    ioc4reg_t intr_clear;
    ioc4reg_t intr_all;
    char      rs422_select_pin;
};

static struct hooks hooks_array[4] =
{
    /* Values for port 0 */
    {
	IOC4_SIO_IR_S0_DELTA_DCD,
	IOC4_SIO_IR_S0_DELTA_CTS,
	IOC4_SIO_IR_S0_TX_MT,
	IOC4_SIO_IR_S0_RX_TIMER,
	IOC4_SIO_IR_S0_RX_HIGH,
	IOC4_SIO_IR_S0_TX_EXPLICIT,
	IOC4_OTHER_IR_S0_MEMERR,
	(IOC4_SIO_IR_S0_TX_MT | IOC4_SIO_IR_S0_RX_FULL |
         IOC4_SIO_IR_S0_RX_HIGH | IOC4_SIO_IR_S0_RX_TIMER |
         IOC4_SIO_IR_S0_DELTA_DCD | IOC4_SIO_IR_S0_DELTA_CTS |
	 IOC4_SIO_IR_S0_INT | IOC4_SIO_IR_S0_TX_EXPLICIT),
	IOC4_SIO_IR_S0,
	IOC4_GPPR_UART0_MODESEL_PIN,
    },

    /* Values for port 1 */
    {
	IOC4_SIO_IR_S1_DELTA_DCD,
	IOC4_SIO_IR_S1_DELTA_CTS,
	IOC4_SIO_IR_S1_TX_MT,
	IOC4_SIO_IR_S1_RX_TIMER,
	IOC4_SIO_IR_S1_RX_HIGH,
	IOC4_SIO_IR_S1_TX_EXPLICIT,
	IOC4_OTHER_IR_S1_MEMERR,
	(IOC4_SIO_IR_S1_TX_MT | IOC4_SIO_IR_S1_RX_FULL |
         IOC4_SIO_IR_S1_RX_HIGH | IOC4_SIO_IR_S1_RX_TIMER |
         IOC4_SIO_IR_S1_DELTA_DCD | IOC4_SIO_IR_S1_DELTA_CTS |
	 IOC4_SIO_IR_S1_INT | IOC4_SIO_IR_S1_TX_EXPLICIT),
	IOC4_SIO_IR_S1,
	IOC4_GPPR_UART1_MODESEL_PIN,
    },

    /* Values for port 2 */
    {
	IOC4_SIO_IR_S2_DELTA_DCD,
	IOC4_SIO_IR_S2_DELTA_CTS,
	IOC4_SIO_IR_S2_TX_MT,
	IOC4_SIO_IR_S2_RX_TIMER,
	IOC4_SIO_IR_S2_RX_HIGH,
	IOC4_SIO_IR_S2_TX_EXPLICIT,
	IOC4_OTHER_IR_S2_MEMERR,
	(IOC4_SIO_IR_S2_TX_MT | IOC4_SIO_IR_S2_RX_FULL |
         IOC4_SIO_IR_S2_RX_HIGH | IOC4_SIO_IR_S2_RX_TIMER |
         IOC4_SIO_IR_S2_DELTA_DCD | IOC4_SIO_IR_S2_DELTA_CTS |
	 IOC4_SIO_IR_S2_INT | IOC4_SIO_IR_S2_TX_EXPLICIT),
	IOC4_SIO_IR_S2,
	IOC4_GPPR_UART2_MODESEL_PIN,
    },

    /* Values for port 3 */
    {
	IOC4_SIO_IR_S3_DELTA_DCD,
	IOC4_SIO_IR_S3_DELTA_CTS,
	IOC4_SIO_IR_S3_TX_MT,
	IOC4_SIO_IR_S3_RX_TIMER,
	IOC4_SIO_IR_S3_RX_HIGH,
	IOC4_SIO_IR_S3_TX_EXPLICIT,
	IOC4_OTHER_IR_S3_MEMERR,
	(IOC4_SIO_IR_S3_TX_MT | IOC4_SIO_IR_S3_RX_FULL |
         IOC4_SIO_IR_S3_RX_HIGH | IOC4_SIO_IR_S3_RX_TIMER |
         IOC4_SIO_IR_S3_DELTA_DCD | IOC4_SIO_IR_S3_DELTA_CTS |
	 IOC4_SIO_IR_S3_INT | IOC4_SIO_IR_S3_TX_EXPLICIT),
	IOC4_SIO_IR_S3,
	IOC4_GPPR_UART3_MODESEL_PIN,
    }
};

/* Macros to get into the port hooks.  Require a variable called
 * hooks set to port->hooks
 */
#define H_INTR_TX_MT	   hooks->intr_tx_mt
#define H_INTR_RX_TIMER    hooks->intr_rx_timer
#define H_INTR_RX_HIGH	   hooks->intr_rx_high
#define H_INTR_TX_EXPLICIT hooks->intr_tx_explicit
#define H_INTR_DMA_ERROR   hooks->intr_dma_error
#define H_INTR_CLEAR	   hooks->intr_clear
#define H_INTR_DELTA_DCD   hooks->intr_delta_dcd
#define H_INTR_DELTA_CTS   hooks->intr_delta_cts
#define H_INTR_ALL	   hooks->intr_all
#define H_RS422		   hooks->rs422_select_pin

/* A ring buffer entry */
struct ring_entry {
    union {
	struct {
	    uint32_t alldata;
	    uint32_t allsc;
	} all;
	struct {
	    char data[4];	/* data bytes */
	    char sc[4];		/* status/control */
	} s;
    } u;
};

/* Test the valid bits in any of the 4 sc chars using "allsc" member */
#define RING_ANY_VALID \
	((uint32_t) (IOC4_RXSB_MODEM_VALID | IOC4_RXSB_DATA_VALID) * 0x01010101)

#define ring_sc     u.s.sc
#define ring_data   u.s.data
#define ring_allsc  u.all.allsc

/* Number of entries per ring buffer. */
#define ENTRIES_PER_RING (RING_BUF_SIZE / (int) sizeof(struct ring_entry))

/* An individual ring */
struct ring {
    struct ring_entry entries[ENTRIES_PER_RING];
};

/* The whole enchilada */
struct ring_buffer {
        struct ring TX_0_OR_2;
        struct ring RX_0_OR_2;
        struct ring TX_1_OR_3;
        struct ring RX_1_OR_3;
};

/* Get a ring from a port struct */
#define RING(port, which) \
    &(((struct ring_buffer *) ((port)->ip_ring_buf_k0))->which)

/* Local functions: */
static int ioc4_open		(sioport_t *port);
static int ioc4_config		(sioport_t *port, int baud, int byte_size,
				 int stop_bits, int parenb, int parodd);
static int ioc4_enable_hfc	(sioport_t *port, int enable);
static int ioc4_set_extclk	(sioport_t *port, int clock_factor);

/* Data transmission */
static int do_ioc4_write	(sioport_t *port, char *buf, int len);
static int ioc4_write		(sioport_t *port, char *buf, int len);
static int ioc4_sync_write	(sioport_t *port, char *buf, int len);
static void ioc4_wrflush	(sioport_t *port);
static int ioc4_break		(sioport_t *port, int brk);
static int ioc4_enable_tx	(sioport_t *port, int enb);

/* Data reception */
static int ioc4_read		(sioport_t *port, char *buf, int len);

/* Event notification */
static int ioc4_notification	(sioport_t *port, int mask, int on);
static int ioc4_rx_timeout	(sioport_t *port, int timeout);

/* Modem control */
static int ioc4_set_DTR		(sioport_t *port, int dtr);
static int ioc4_set_RTS		(sioport_t *port, int rts);
static int ioc4_query_DCD	(sioport_t *port);
static int ioc4_query_CTS	(sioport_t *port);

/* Output mode */
static int ioc4_set_proto	(sioport_t *port, enum sio_proto proto);

/* User mapped driver support */
static int ioc4_get_mapid	(sioport_t *port, void *arg);
static int ioc4_set_sscr	(sioport_t *port, int arg, int flag);

static struct serial_calldown ioc4_calldown = {
    ioc4_open,
    ioc4_config,
    ioc4_enable_hfc,
    ioc4_set_extclk,
    ioc4_write,
    ioc4_sync_write,
    ioc4_wrflush,	/* du flush */
    ioc4_break,
    ioc4_enable_tx,
    ioc4_read,
    ioc4_notification,
    ioc4_rx_timeout,
    ioc4_set_DTR,
    ioc4_set_RTS,
    ioc4_query_DCD,
    ioc4_query_CTS,
    ioc4_set_proto,
    ioc4_get_mapid,
    0,
    0,
    ioc4_set_sscr
};

/* Baud rate stuff */
#define SET_BAUD(p, b) set_baud_ti(p, b)
static int set_baud_ti(ioc4port_t *, int);

#ifdef DEBUG
/* Performance characterization logging */
#define DEBUGINC(x,i) stats.x += i

static struct {

    /* Ports present */
    uint ports;

    /* Ports killed */
    uint killed;

    /* Interrupt counts */
    uint total_intr;
    uint port_0_intr;
    uint port_1_intr;
    uint ddcd_intr;
    uint dcts_intr;
    uint rx_timer_intr;
    uint rx_high_intr;
    uint explicit_intr;
    uint mt_intr;
    uint mt_lowat_intr;

    /* Write characteristics */
    uint write_bytes;
    uint write_cnt;
    uint wrote_bytes;
    uint tx_buf_used;
    uint tx_buf_cnt;
    uint tx_pio_cnt;691

    /* Read characteristics */
    uint read_bytes;
    uint read_cnt;
    uint drain;
    uint drainwait;
    uint resetdma;
    uint read_ddcd;
    uint rx_overrun;
    uint parity;
    uint framing;
    uint brk;
    uint red_bytes;
    uint rx_buf_used;
    uint rx_buf_cnt;

    /* Errors */
    uint dma_lost;
    uint read_aborted;
    uint read_aborted_detected;
} stats;

#else
#define DEBUGINC(x,i)
#endif

/* Infinite loop detection.
 */
#define MAXITER 1000000
#define SPIN(cond, success) \
{ \
	 int spiniter = 0; \
	 success = 1; \
	 while(cond) { \
		 spiniter++; \
		 if (spiniter > MAXITER) { \
			 success = 0; \
			 break; \
		 } \
	 } \
}


static iopaddr_t
ring_dmatrans(vertex_hdl_t conn_vhdl, caddr_t vaddr)
{
    extern iopaddr_t pciio_dma_addr (vertex_hdl_t, device_desc_t, paddr_t,
                                     size_t, pciio_dmamap_t *, unsigned);
    iopaddr_t	paddr = (iopaddr_t)vaddr;

    if (conn_vhdl != GRAPH_VERTEX_NONE)
#ifdef	USE_64BIT_DMA
        /* Use 64-bit DMA address when the IOC4 supports it */
        return pciio_dmatrans_addr (conn_vhdl, 0, paddr, TOTAL_RING_BUF_SIZE, PCIIO_DMA_A64 | PCIIO_BYTE_STREAM);

#else
        /* Use 32-bit DMA address for current IOC4 */ 
	return pciio_dma_addr (conn_vhdl, 0, paddr, TOTAL_RING_BUF_SIZE, NULL, PCIIO_BYTE_STREAM);
#endif

    return paddr;
}


/* If interrupt routine called enable_intrs, then would need to write
 * mask_enable_intrs() routine.
 */
static inline void
mask_disable_intrs(ioc4port_t *port, ioc4reg_t mask)
{
    port->ip_ienb &= ~mask;
}


static void
enable_intrs(ioc4port_t *port, ioc4reg_t mask)
{
    struct hooks *hooks = port->ip_hooks;
 
    if ((port->ip_ienb & mask) != mask) {
	IOC4_WRITE_IES(port->ip_ioc4_soft, mask, ioc4_sio_intr_type);
	port->ip_ienb |= mask;
    }

    if (port->ip_ienb)
        IOC4_WRITE_IES(port->ip_ioc4_soft, H_INTR_DMA_ERROR, ioc4_other_intr_type);
}


static void
disable_intrs(ioc4port_t *port, ioc4reg_t mask)
{
    struct hooks *hooks = port->ip_hooks;

    if (port->ip_ienb & mask) {
	IOC4_WRITE_IEC(port->ip_ioc4_soft, mask, ioc4_sio_intr_type);
	port->ip_ienb &= ~mask;
    }

    if (!port->ip_ienb)
        IOC4_WRITE_IEC(port->ip_ioc4_soft, H_INTR_DMA_ERROR, ioc4_other_intr_type);
}


/* Service any pending interrupts on the given port */
static void
ioc4_serial_intr(intr_arg_t arg, ioc4reg_t sio_ir)
{
    ioc4port_t   *port = (ioc4port_t *) arg;
    sioport_t    *gp = GPORT(port);
    struct hooks *hooks = port->ip_hooks;
    unsigned      rx_high_rd_aborted = 0;
    unsigned int  flags;

    PROGRESS();
#ifdef NOT_YET
    ASSERT(sio_port_islocked(gp) == 0);
#endif

    /* Possible race condition here: The TX_MT interrupt bit may be
     * cleared without the intervention of the interrupt handler,
     * e.g. by a write.  If the top level interrupt handler reads a
     * TX_MT, then some other processor does a write, starting up
     * output, then we come in here, see the TX_MT and stop DMA, the
     * output started by the other processor will hang.  Thus we can
     * only rely on TX_MT being legitimate if it is read while the
     * port lock is held.  Therefore this bit must be ignored in the
     * passed in interrupt mask which was read by the top level
     * interrupt handler since the port lock was not held at the time
     * it was read.  We can only rely on this bit being accurate if it
     * is read while the port lock is held.  So we'll clear it for now,
     * and reload it later once we have the port lock.
     */
    sio_ir &= ~(H_INTR_TX_MT);

    SIO_LOCK_PORT(gp, flags);

    dprintf(("interrupt: sio_ir 0x%x\n", sio_ir));

    do {
	ioc4reg_t shadow;

	/* Handle a DCD change */
	if (sio_ir & H_INTR_DELTA_DCD) {
	    DEBUGINC(ddcd_intr, 1);

	    PROGRESS();
	    /* ACK the interrupt */
	    PCI_OUTW(&port->ip_ioc4->sio_ir, H_INTR_DELTA_DCD);

	    /* If DCD has raised, notify upper layer.  Otherwise
	     * wait for a record to be posted to notify of a dropped DCD.
	     */
	    shadow = PCI_INW(&port->ip_serial->shadow);
	    
            if (port->ip_notify & N_DDCD) {
		PROGRESS();
                if (shadow & IOC4_SHADOW_DCD)   /* Notify upper layer of DCD */
                        UP_DDCD(gp, 1);
                else
                        port->ip_flags |= DCD_ON;  /* Flag delta DCD/no DCD */
	    }
	}

	/* Handle a CTS change */
	if (sio_ir & H_INTR_DELTA_CTS) {
	    DEBUGINC(dcts_intr, 1);
	    PROGRESS();

	    /* ACK the interrupt */
	    PCI_OUTW(&port->ip_ioc4->sio_ir, H_INTR_DELTA_CTS);

	    shadow = PCI_INW(&port->ip_serial->shadow);

	    /* Notify upper layer */
	    if (port->ip_notify & N_DCTS) {
		if (shadow & IOC4_SHADOW_CTS)
		    UP_DCTS(gp, 1);
		else
		    UP_DCTS(gp, 0);
	    }
	}

	/* RX timeout interrupt.  Must be some data available.  Put this
	 * before the check for RX_HIGH since servicing this condition
	 * may cause that condition to clear.
	 */
	if (sio_ir & H_INTR_RX_TIMER) {
	    PROGRESS();
	    DEBUGINC(rx_timer_intr, 1);

	    /* ACK the interrupt */
	    PCI_OUTW(&port->ip_ioc4->sio_ir, H_INTR_RX_TIMER);

	    if (port->ip_notify & N_DATA_READY)
		UP_DATA_READY(gp);
	}

	/* RX high interrupt. Must be after RX_TIMER.
	 */
	else if (sio_ir & H_INTR_RX_HIGH) {
	    DEBUGINC(rx_high_intr, 1);

	    PROGRESS();
	    /* Data available, notify upper layer */
	    if (port->ip_notify & N_DATA_READY)
		UP_DATA_READY(gp);

	    /* We can't ACK this interrupt.  If up_data_ready didn't
	     * cause the condition to clear, we'll have to disable
	     * the interrupt until the data is drained by the upper layer.
	     * If the read was aborted, don't disable the interrupt as
	     * this may cause us to hang indefinitely.  An aborted read
	     * generally means that this interrupt hasn't been delivered
	     * to the cpu yet anyway, even though we see it as asserted 
	     * when we read the sio_ir.
	     */
	    if ((sio_ir = PENDING(port)) & H_INTR_RX_HIGH) {
		PROGRESS();
		if ((port->ip_flags & READ_ABORTED) == 0) {
		    mask_disable_intrs(port, H_INTR_RX_HIGH);
		    port->ip_flags |= INPUT_HIGH;
		}
		else {
		    DEBUGINC(read_aborted_detected, 1);
		    /* We will be stuck in this loop forever,
		     * higher level will never get time to finish
		     */
		    rx_high_rd_aborted++;
		}
	    }
	}

	/* We got a low water interrupt: notify upper layer to
	 * send more data.  Must come before TX_MT since servicing
	 * this condition may cause that condition to clear.
	 */
	if (sio_ir & H_INTR_TX_EXPLICIT) {
	    DEBUGINC(explicit_intr, 1);
	    PROGRESS();

	    port->ip_flags &= ~LOWAT_WRITTEN;

	    /* ACK the interrupt */
	    PCI_OUTW(&port->ip_ioc4->sio_ir, H_INTR_TX_EXPLICIT);

	    if (port->ip_notify & N_OUTPUT_LOWAT)
		UP_OUTPUT_LOWAT(gp);
	}

	/* Handle TX_MT.  Must come after TX_EXPLICIT.
	 */
	else if (sio_ir & H_INTR_TX_MT) {
	    DEBUGINC(mt_intr, 1);
	    PROGRESS();

	    /* If the upper layer is expecting a lowat notification
	     * and we get to this point it probably means that for
	     * some reason the TX_EXPLICIT didn't work as expected
	     * (that can legitimately happen if the output buffer is
	     * filled up in just the right way).  So sent the notification
	     * now.
	     */
	    if (port->ip_notify & N_OUTPUT_LOWAT) {
		DEBUGINC(mt_lowat_intr, 1);
		PROGRESS();

		if (port->ip_notify & N_OUTPUT_LOWAT)
		    UP_OUTPUT_LOWAT(gp);

		/* We need to reload the sio_ir since the upcall may
		 * have caused another write to occur, clearing
		 * the TX_MT condition.
		 */
		sio_ir = PENDING(port);
	    }

	    /* If the TX_MT condition still persists even after the upcall,
	     * we've got some work to do.
	     */
	    if (sio_ir & H_INTR_TX_MT) {

		PROGRESS();

		/* If we are not currently expecting DMA input, and the
		 * transmitter has just gone idle, there is no longer any
		 * reason for DMA, so disable it.
		 */
		if (!(port->ip_notify & (N_DATA_READY | N_DDCD))) {
		    ASSERT(port->ip_sscr & IOC4_SSCR_DMA_EN);
		    port->ip_sscr &= ~IOC4_SSCR_DMA_EN;
		    PCI_OUTW(&port->ip_serial->sscr, port->ip_sscr);
		}

		/* Prevent infinite TX_MT interrupt */
		mask_disable_intrs(port, H_INTR_TX_MT);
	    }
	}

	sio_ir = PENDING(port);

	/* if the read was aborted and only H_INTR_RX_HIGH,
	 * clear H_INTR_RX_HIGH, so we do not loop forever.
	 */

	if ( rx_high_rd_aborted && (sio_ir == H_INTR_RX_HIGH) ) {
	    sio_ir &= ~H_INTR_RX_HIGH;
	}
    } while (sio_ir & H_INTR_ALL);

    SIO_UNLOCK_PORT(gp, flags);

    /* Re-enable interrupts before returning from interrupt handler.
     * Getting interrupted here is okay.  It'll just v() our semaphore, and
     * we'll come through the loop again.
     */

    IOC4_WRITE_IES(port->ip_ioc4_soft, port->ip_ienb, ioc4_sio_intr_type);
}


/*ARGSUSED*/

/* Service any pending DMA error interrupts on the given port */
static void
ioc4_dma_error_intr(intr_arg_t arg, ioc4reg_t other_ir)
{
    ioc4port_t   *port = (ioc4port_t *) arg;
    sioport_t    *gp = GPORT(port);
    struct hooks *hooks = port->ip_hooks;
    unsigned int flags;

    SIO_LOCK_PORT(gp, flags);

    dprintf(("interrupt: other_ir 0x%x\n", other_ir));

    /* ACK the interrupt */
    PCI_OUTW(&port->ip_ioc4->other_ir, H_INTR_DMA_ERROR);

    printk( "DMA error on serial port %p\n", (void *)port->ip_port_vhdl);

    if (port->ip_ioc4->pci_err_addr_l & IOC4_PCI_ERR_ADDR_VLD) {
        printk( "PCI error address is 0x%lx, master is serial port %c %s\n", 
                ((uint64_t) port->ip_ioc4->pci_err_addr_h << 32) |
                (port->ip_ioc4->pci_err_addr_l & IOC4_PCI_ERR_ADDR_ADDR_MSK),
                '1' + (char) ((port->ip_ioc4->pci_err_addr_l &
                                         IOC4_PCI_ERR_ADDR_MST_NUM_MSK) >> 1),
                (port->ip_ioc4->pci_err_addr_l & IOC4_PCI_ERR_ADDR_MST_TYP_MSK) 
                                                               ? "RX" : "TX");

        if (port->ip_ioc4->pci_err_addr_l & IOC4_PCI_ERR_ADDR_MUL_ERR)
            printk( "Multiple errors occurred\n"); 
    }

    SIO_UNLOCK_PORT(gp, flags);

    /* Re-enable DMA error interrupts */
    IOC4_WRITE_IES(port->ip_ioc4_soft, H_INTR_DMA_ERROR, ioc4_other_intr_type);
}


/* Baud rate setting code */
static int
set_baud_ti(ioc4port_t *port, int baud)
{
    int            actual_baud;
    int            diff;
    int            lcr;
    unsigned short divisor;

    divisor = SER_DIVISOR(baud, IOC4_SER_XIN_CLK);
    if (!divisor)
        return(1);
    actual_baud = DIVISOR_TO_BAUD(divisor, IOC4_SER_XIN_CLK);

    diff = actual_baud - baud;
    if (diff < 0)
        diff = -diff;

    /* If we're within 1%, we've found a match */
    if (diff * 100 > actual_baud)
        return(1);

    lcr = PCI_INB(&port->ip_uart->i4u_lcr);

    PCI_OUTB(&port->ip_uart->i4u_lcr, lcr | LCR_DLAB);

    PCI_OUTB(&port->ip_uart->i4u_dll, (char) divisor); 

    PCI_OUTB(&port->ip_uart->i4u_dlm, (char) (divisor >> 8));

    PCI_OUTB(&port->ip_uart->i4u_lcr, lcr);

    return(0);
}


/* Initialize the sio and ioc4 hardware for a given port */
static int
hardware_init(ioc4port_t *port)
{
    ioc4reg_t     sio_cr;
    struct hooks *hooks = port->ip_hooks;

    DEBUGINC(ports, 1);

    /* Idle the IOC4 serial interface */
    PCI_OUTW(&port->ip_serial->sscr, IOC4_SSCR_RESET);

    /* Wait until any pending bus activity for this port has ceased */
    do sio_cr = PCI_INW(&port->ip_ioc4->sio_cr);
    while(!(sio_cr & IOC4_SIO_CR_SIO_DIAG_IDLE));

    /* Finish reset sequence */
    PCI_OUTW(&port->ip_serial->sscr, 0);

    /* Once RESET is done, reload cached tx_prod and rx_cons values
     * and set rings to empty by making prod == cons
     */
    port->ip_tx_prod = PCI_INW(&port->ip_serial->stcir) & PROD_CONS_MASK;
    PCI_OUTW(&port->ip_serial->stpir, port->ip_tx_prod);

    port->ip_rx_cons = PCI_INW(&port->ip_serial->srpir) & PROD_CONS_MASK;
    PCI_OUTW(&port->ip_serial->srcir, port->ip_rx_cons);

    /* Disable interrupts for this 16550 */
    PCI_OUTB(&port->ip_uart->i4u_lcr, 0); /* clear DLAB */
    PCI_OUTB(&port->ip_uart->i4u_ier, 0);

    /* Set the default baud */
    SET_BAUD(port, port->ip_baud);

    /* Set line control to 8 bits no parity */
    PCI_OUTB(&port->ip_uart->i4u_lcr, LCR_BITS8 | LCR_1_STOP_BITS);
   
    /* Enable the FIFOs */ 
    PCI_OUTB(&port->ip_uart->i4u_fcr, FCR_FIFOEN);
    /* then reset 16550 FIFOs */
    PCI_OUTB(&port->ip_uart->i4u_fcr,
             FCR_FIFOEN | FCR_RxFIFO | FCR_TxFIFO);

    /* Clear modem control register */
    PCI_OUTB(&port->ip_uart->i4u_mcr, 0);

    /* Clear deltas in modem status register */
    PCI_INB(&port->ip_uart->i4u_msr);

    /* Only do this once per port pair */
    if (port->ip_hooks == &hooks_array[0] || port->ip_hooks == &hooks_array[2]) {
	iopaddr_t             ring_pci_addr;
        volatile ioc4reg_t   *sbbr_l;
        volatile ioc4reg_t   *sbbr_h;

        if(port->ip_hooks == &hooks_array[0]) {
		sbbr_l = &port->ip_ioc4->sbbr01_l;
		sbbr_h = &port->ip_ioc4->sbbr01_h;
        }
	else {
		sbbr_l = &port->ip_ioc4->sbbr23_l;
		sbbr_h = &port->ip_ioc4->sbbr23_h;
	}

	/* Set the DMA address */
	ring_pci_addr = ring_dmatrans(port->ip_conn_vhdl,
				      port->ip_ring_buf_k0);

	PCI_OUTW(sbbr_h,
		 (ioc4reg_t) ((__psunsigned_t) ring_pci_addr >> 32));

	PCI_OUTW(sbbr_l,
		 ((ioc4reg_t) (int64_t) ring_pci_addr | IOC4_BUF_SIZE_BIT));

#ifdef IOC4_SIO_DEBUG
	{
		unsigned int tmp1, tmp2;

		tmp1 = PCI_INW(sbbr_l);
		tmp2 = PCI_INW(sbbr_h);
		printk("========== %s : sbbr_l [%p]/0x%x sbbr_h [%p]/0x%x\n",
				__FUNCTION__, (void *)sbbr_l, tmp1, (void *)sbbr_h, tmp2);
	}
#endif
    }

    /* Set the receive timeout value to 10 msec */
    PCI_OUTW(&port->ip_serial->srtr, IOC4_SRTR_HZ / 100);

    /* Set RX threshold, enable DMA */
    /* Set high water mark at 3/4 of full ring */
    port->ip_sscr = (ENTRIES_PER_RING * 3 / 4);

    PCI_OUTW(&port->ip_serial->sscr, port->ip_sscr);

    /* Disable and clear all serial related interrupt bits */
    IOC4_WRITE_IEC(port->ip_ioc4_soft, H_INTR_CLEAR, ioc4_sio_intr_type);
    port->ip_ienb &= ~H_INTR_CLEAR;
    PCI_OUTW(&port->ip_ioc4->sio_ir, H_INTR_CLEAR);

    return(0);
}


/*
 * Device initialization.
 * Called at *_attach() time for each
 * IOC4 with serial ports in the system.
 * If vhdl is GRAPH_VERTEX_NONE, do not do
 * any graph related work; otherwise, it
 * is the IOC4 vertex that should be used
 * for requesting pciio services.
 */
int
ioc4_serial_attach(vertex_hdl_t conn_vhdl, void *ioc4)
{
    /*REFERENCED*/
    graph_error_t	rc;
    ioc4_mem_t	       *ioc4_mem;
    vertex_hdl_t	port_vhdl, ioc4_vhdl;
    vertex_hdl_t	intr_dev_vhdl;
    ioc4port_t	       *port;
    ioc4port_t	       *ports[4];
    static char	       *names[] = { "tty/1", "tty/2", "tty/3", "tty/4" };
    int			x, first_port = -1, last_port = -1;
    void	       *ioc4_soft;
    unsigned int        ioc4_revid_min = 62;
    unsigned int        ioc4_revid;


    /* IOC4 firmware must be at least rev 62 */
    ioc4_revid = pciio_config_get(conn_vhdl, PCI_CFG_REV_ID, 1); 

    if (ioc4_revid < ioc4_revid_min) {
        printk( "IOC4 serial ports not supported on firmware rev %d, please upgrade to rev %d or higher\n", ioc4_revid, ioc4_revid_min);
        return -1;
    }

    first_port = 0;
    last_port = 3;

    /* Get back pointer to the ioc4 soft area */
    rc = hwgraph_traverse(conn_vhdl, EDGE_LBL_IOC4, &ioc4_vhdl);
    ASSERT(rc == GRAPH_SUCCESS);
    ioc4_soft = (void *)hwgraph_fastinfo_get(ioc4_vhdl);

    /* grab the PIO address */
    ioc4_mem = (ioc4_mem_t *)ioc4;
    ASSERT(ioc4_mem != NULL);

    /*
     * Create port structures for each port
     */
    NEWA(port, 4);
#ifdef IOC4_SIO_DEBUG
    printk("%s : [addr 0x%p]\n", __FUNCTION__, (void *)port);
#endif
    ports[0] = port++;
    ports[1] = port++;
    ports[2] = port++;
    ports[3] = port++;

#if DEBUG
    {
	int slot = atomicAddInt(&next_saveport, 4) - 4;
	saveport[slot] = ports[0];
	saveport[slot + 1] = ports[1];
	saveport[slot + 2] = ports[2];
	saveport[slot + 3] = ports[3];
	ASSERT(slot < MAXSAVEPORT);
    }
#endif

#ifdef DEBUG
    if ((caddr_t) port != (caddr_t) &(port->ip_sioport))
	panic("sioport is not first member of ioc4port struct\n");
#endif

    /* Allocate buffers and jumpstart the hardware.
     */
    for (x = first_port; x < (last_port + 1); x++) {

	port = ports[x];
#ifdef IOC4_SIO_DEBUG
	printk("%s : initialize port %d [addr 0x%p/0x%p]\n", __FUNCTION__, x, (void *)port,
								(void *)GPORT(port));
#endif
	port->ip_ioc4_soft = ioc4_soft;
	rc = hwgraph_path_add(conn_vhdl, names[x], &port_vhdl);
	ASSERT(rc == GRAPH_SUCCESS);
	port->ip_conn_vhdl = conn_vhdl;
	port->ip_port_vhdl = port_vhdl;
	port->ip_ienb	= 0;
	hwgraph_fastinfo_set(port_vhdl, (arbitrary_info_t) port);

	/* Perform upper layer initialization. Create all device node
	 * types including rs422 ports.
	 */
	ioc4_serial_initport(GPORT(port), x);
	port->ip_baud = 9600;

	/* Attach the calldown hooks so upper layer can call our
	 * routines.
	 */
	port->ip_sioport.sio_calldown = &ioc4_calldown;

	/* Map in the IOC4 register area */
	port->ip_ioc4 = ioc4_mem;
    }

    {
	/* Port 0 */
	port = ports[0];
	port->ip_hooks = &hooks_array[0];

	/* Get direct hooks to the serial regs and uart regs
	 * for this port
	 */
	port->ip_serial = &(port->ip_ioc4->port_0);
	port->ip_uart = &(port->ip_ioc4->uart_0);
#ifdef IOC4_SIO_DEBUG
	printk("==== %s : serial port 0 address 0x%p uart address 0x%p\n",
				__FUNCTION__, (void *)port->ip_serial, (void *)port->ip_uart);
#endif

	/* If we don't already have a ring buffer,
	 * set one up.
	 */
	if (port->ip_ring_buf_k0 == 0) {

#if PAGE_SIZE >= TOTAL_RING_BUF_SIZE
	    if ((port->ip_ring_buf_k0 = kvpalloc(1, VM_DIRECT, 0)) == 0)
		panic("ioc4_uart driver cannot allocate page\n");
#else
	    /* We need to allocate a chunk of memory on a
	     * TOTAL_RING_BUF_SIZE boundary.
	     */
	    {
		pgno_t pfn;
		caddr_t vaddr;
		if ((pfn = contig_memalloc(TOTAL_RING_BUF_SIZE / PAGE_SIZE,
					   TOTAL_RING_BUF_SIZE / PAGE_SIZE,
					   VM_DIRECT)) == 0)
		    panic("ioc4_uart driver cannot allocate page\n");
		ASSERT(small_pfn(pfn));
		vaddr = small_pfntova_K0(pfn);
		(void) COLOR_VALIDATION(pfdat + pfn,
					colorof(vaddr),
					0, VM_DIRECT);
		port->ip_ring_buf_k0 = vaddr;
	    }
#endif
	}
	ASSERT((((int64_t)port->ip_ring_buf_k0) &
		(TOTAL_RING_BUF_SIZE - 1)) == 0);
	memset(port->ip_ring_buf_k0, 0, TOTAL_RING_BUF_SIZE);
	port->ip_inring = RING(port, RX_0_OR_2);
	port->ip_outring = RING(port, TX_0_OR_2);

	/* Initialize the hardware for IOC4 */
	hardware_init(port);

	if (hwgraph_edge_get(ports[0]->ip_port_vhdl, "d", &intr_dev_vhdl) !=
							GRAPH_SUCCESS) {
	    intr_dev_vhdl = ports[0]->ip_port_vhdl;
	}

	/* Attach interrupt handlers */
	ioc4_intr_connect(conn_vhdl,
			  ioc4_sio_intr_type,
			  IOC4_SIO_IR_S0,
			  ioc4_serial_intr,
			  ports[0],
			  ports[0]->ip_port_vhdl,
			  intr_dev_vhdl);

	ioc4_intr_connect(conn_vhdl,
			  ioc4_other_intr_type,
			  IOC4_OTHER_IR_S0_MEMERR,
			  ioc4_dma_error_intr,
			  ports[0],
			  ports[0]->ip_port_vhdl,
			  intr_dev_vhdl);
    }

    {

	/* Port 1 */
	port = ports[1];
	port->ip_hooks = &hooks_array[1];

	port->ip_serial = &(port->ip_ioc4->port_1);
	port->ip_uart = &(port->ip_ioc4->uart_1);
#ifdef IOC4_SIO_DEBUG
	printk("==== %s : serial port 1 address 0x%p uart address 0x%p\n",
				__FUNCTION__, (void *)port->ip_serial, (void *)port->ip_uart);
#endif

	port->ip_ring_buf_k0 = ports[0]->ip_ring_buf_k0;
	port->ip_inring = RING(port, RX_1_OR_3);
	port->ip_outring = RING(port, TX_1_OR_3);

	/* Initialize the hardware for IOC4 */
	hardware_init(port);

	if (hwgraph_edge_get(ports[1]->ip_port_vhdl, "d", &intr_dev_vhdl) !=
							GRAPH_SUCCESS) {
	    intr_dev_vhdl = ports[1]->ip_port_vhdl;
	}

	/* Attach interrupt handler */
	ioc4_intr_connect(conn_vhdl,
		          ioc4_sio_intr_type,
		          IOC4_SIO_IR_S1,
		          ioc4_serial_intr,
		          ports[1],
		          ports[1]->ip_port_vhdl,
		          intr_dev_vhdl);

	ioc4_intr_connect(conn_vhdl,
			  ioc4_other_intr_type,
			  IOC4_OTHER_IR_S1_MEMERR,
			  ioc4_dma_error_intr,
			  ports[1],
			  ports[1]->ip_port_vhdl,
			  intr_dev_vhdl);
    }

    {

	/* Port 2 */
	port = ports[2];
	port->ip_hooks = &hooks_array[2];

	/* Get direct hooks to the serial regs and uart regs
	 * for this port
	 */
	port->ip_serial = &(port->ip_ioc4->port_2);
	port->ip_uart = &(port->ip_ioc4->uart_2);
#ifdef IOC4_SIO_DEBUG
	printk("==== %s : serial port 2 address 0x%p uart address 0x%p\n",
				__FUNCTION__, (void *)port->ip_serial, (void *)port->ip_uart);
#endif

	/* If we don't already have a ring buffer,
	 * set one up.
	 */
	if (port->ip_ring_buf_k0 == 0) {

#if PAGE_SIZE >= TOTAL_RING_BUF_SIZE
	    if ((port->ip_ring_buf_k0 = kvpalloc(1, VM_DIRECT, 0)) == 0)
		panic("ioc4_uart driver cannot allocate page\n");
#else

	    /* We need to allocate a chunk of memory on a
	     * TOTAL_RING_BUF_SIZE boundary.
	     */
	    {
		pgno_t pfn;
		caddr_t vaddr;
		if ((pfn = contig_memalloc(TOTAL_RING_BUF_SIZE / PAGE_SIZE,
					   TOTAL_RING_BUF_SIZE / PAGE_SIZE,
					   VM_DIRECT)) == 0)
		    panic("ioc4_uart driver cannot allocate page\n");
		ASSERT(small_pfn(pfn));
		vaddr = small_pfntova_K0(pfn);
		(void) COLOR_VALIDATION(pfdat + pfn,
					colorof(vaddr),
					0, VM_DIRECT);
		port->ip_ring_buf_k0 = vaddr;
	    }
#endif

	}
	ASSERT((((int64_t)port->ip_ring_buf_k0) &
		(TOTAL_RING_BUF_SIZE - 1)) == 0);
	memset(port->ip_ring_buf_k0, 0, TOTAL_RING_BUF_SIZE);
	port->ip_inring = RING(port, RX_0_OR_2);
	port->ip_outring = RING(port, TX_0_OR_2);

	/* Initialize the hardware for IOC4 */
	hardware_init(port);

	if (hwgraph_edge_get(ports[0]->ip_port_vhdl, "d", &intr_dev_vhdl) !=
							GRAPH_SUCCESS) {
	    intr_dev_vhdl = ports[2]->ip_port_vhdl;
	}

	/* Attach interrupt handler */
	ioc4_intr_connect(conn_vhdl,
			  ioc4_sio_intr_type,
			  IOC4_SIO_IR_S2,
			  ioc4_serial_intr,
			  ports[2],
			  ports[2]->ip_port_vhdl,
			  intr_dev_vhdl);

	ioc4_intr_connect(conn_vhdl,
			  ioc4_other_intr_type,
			  IOC4_OTHER_IR_S2_MEMERR,
			  ioc4_dma_error_intr,
			  ports[2],
			  ports[2]->ip_port_vhdl,
			  intr_dev_vhdl);
    }

    {

	/* Port 3 */
	port = ports[3];
	port->ip_hooks = &hooks_array[3];

	port->ip_serial = &(port->ip_ioc4->port_3);
	port->ip_uart = &(port->ip_ioc4->uart_3);
#ifdef IOC4_SIO_DEBUG
	printk("==== %s : serial port 3 address 0x%p uart address 0x%p\n",
				__FUNCTION__, (void *)port->ip_serial, (void *)port->ip_uart);
#endif

	port->ip_ring_buf_k0 = ports[2]->ip_ring_buf_k0;
	port->ip_inring = RING(port, RX_1_OR_3);
	port->ip_outring = RING(port, TX_1_OR_3);

	/* Initialize the hardware for IOC4 */
	hardware_init(port);

	if (hwgraph_edge_get(ports[3]->ip_port_vhdl, "d", &intr_dev_vhdl) !=
							GRAPH_SUCCESS) {
	    intr_dev_vhdl = ports[3]->ip_port_vhdl;
	}

	/* Attach interrupt handler */
	ioc4_intr_connect(conn_vhdl,
		          ioc4_sio_intr_type,
		          IOC4_SIO_IR_S3,
		          ioc4_serial_intr,
		          ports[3],
		          ports[3]->ip_port_vhdl,
		          intr_dev_vhdl);

	ioc4_intr_connect(conn_vhdl,
			  ioc4_other_intr_type,
			  IOC4_OTHER_IR_S3_MEMERR,
			  ioc4_dma_error_intr,
			  ports[3],
			  ports[3]->ip_port_vhdl,
			  intr_dev_vhdl);
    }

#ifdef	DEBUG
    idbg_addfunc( "ioc4dump", idbg_ioc4dump );
#endif

    return 0;
}


/* Shut down an IOC4 */
/* ARGSUSED1 */
void
ioc4_serial_kill(ioc4port_t *port)
{
    DEBUGINC(killed, 1);

    /* Notify upper layer that this port is no longer usable */
    UP_DETACH(GPORT(port));

    /* Clear everything in the sscr */
    PCI_OUTW(&port->ip_serial->sscr, 0);
    port->ip_sscr = 0;

#ifdef DEBUG
    /* Make sure nobody gets past the lock and accesses the hardware */
    port->ip_ioc4 = 0;
    port->ip_serial = 0;
#endif

}


/*
 * Open a port
 */
static int
ioc4_open(sioport_t *port)
{
    ioc4port_t *p = LPORT(port);
    int         spin_success;

#ifdef NOT_YET
    ASSERT(L_LOCKED(port, L_OPEN));
#endif

    p->ip_flags = 0;
    p->ip_modem_bits = 0;

    /* Pause the DMA interface if necessary */
    if (p->ip_sscr & IOC4_SSCR_DMA_EN) {
	PCI_OUTW(&p->ip_serial->sscr, p->ip_sscr | IOC4_SSCR_DMA_PAUSE);
	SPIN((PCI_INW(&p->ip_serial->sscr) & IOC4_SSCR_PAUSE_STATE) == 0,
             spin_success);
	if (!spin_success) {
		NOT_PROGRESS();
		return(-1);
	}
    }

    /* Reset the input fifo.  If the uart received chars while the port
     * was closed and DMA is not enabled, the uart may have a bunch of
     * chars hanging around in its RX fifo which will not be discarded
     * by rclr in the upper layer. We must get rid of them here.
     */
    PCI_OUTB(&p->ip_uart->i4u_fcr, FCR_FIFOEN | FCR_RxFIFO);

    /* Set defaults */
    SET_BAUD(p, 9600);

    PCI_OUTB(&p->ip_uart->i4u_lcr, LCR_BITS8 | LCR_1_STOP_BITS);

    /* Re-enable DMA, set default threshold to intr whenever there is
     * data available.
     */
    p->ip_sscr &= ~IOC4_SSCR_RX_THRESHOLD;
    p->ip_sscr |= 1; /* default threshold */

    /* Plug in the new sscr.  This implicitly clears the DMA_PAUSE
     * flag if it was set above
     */
    PCI_OUTW(&p->ip_serial->sscr, p->ip_sscr);

    PCI_OUTW(&p->ip_serial->srtr, 0);

    p->ip_tx_lowat = 1;

    dprintf(("ioc4 open successful\n"));

    return(0);
}


/*
 * Config hardware
 */
static int
ioc4_config(sioport_t *port,
	    int baud,
	    int byte_size,
	    int stop_bits,
	    int parenb,
	    int parodd)
{
    ioc4port_t *p = LPORT(port);
    char        lcr, sizebits;
    int         spin_success;

#ifdef NOT_YET
    ASSERT(L_LOCKED(port, L_CONFIG));
#endif

    if (SET_BAUD(p, baud))
	return(1);

    switch(byte_size) {
      case 5:
	sizebits = LCR_BITS5;
	break;
      case 6:
	sizebits = LCR_BITS6;
	break;
      case 7:
	sizebits = LCR_BITS7;
	break;
      case 8:
	sizebits = LCR_BITS8;
	break;
      default:
	dprintf(("invalid byte size port 0x%x size %d\n", port, byte_size));
	return(1);
    }

    /* Pause the DMA interface if necessary */
    if (p->ip_sscr & IOC4_SSCR_DMA_EN) {
	PCI_OUTW(&p->ip_serial->sscr, p->ip_sscr | IOC4_SSCR_DMA_PAUSE);
	SPIN((PCI_INW(&p->ip_serial->sscr) & IOC4_SSCR_PAUSE_STATE) == 0,
             spin_success);
	if (!spin_success) 
		return(-1);
    }

    /* Clear relevant fields in lcr */
    lcr = PCI_INB(&p->ip_uart->i4u_lcr);
    lcr &= ~(LCR_MASK_BITS_CHAR | LCR_EPS |
             LCR_PEN | LCR_MASK_STOP_BITS);

    /* Set byte size in lcr */
    lcr |= sizebits;

    /* Set parity */
    if (parenb) {
	lcr |= LCR_PEN;
	if (!parodd)
	    lcr |= LCR_EPS;
    }

    /* Set stop bits */
    if (stop_bits)
	lcr |= LCR_2_STOP_BITS;

    PCI_OUTB(&p->ip_uart->i4u_lcr, lcr);

    dprintf(("ioc4_config: lcr bits 0x%x\n", lcr));

    /* Re-enable the DMA interface if necessary */
    if (p->ip_sscr & IOC4_SSCR_DMA_EN) {
	PCI_OUTW(&p->ip_serial->sscr, p->ip_sscr);
    }

    p->ip_baud = baud;

    /* When we get within this number of ring entries of filling the
     * entire ring on TX, place an EXPLICIT intr to generate a lowat
     * notification when output has drained.
     */
    p->ip_tx_lowat = (TX_LOWAT_CHARS(baud) + 3) / 4;
    if (p->ip_tx_lowat == 0)
	p->ip_tx_lowat = 1;

    ioc4_rx_timeout(port, p->ip_rx_timeout);

    return(0);
}


/*
 * Enable hardware flow control
 */
static int
ioc4_enable_hfc(sioport_t *port, int enable)
{
    ioc4port_t *p = LPORT(port);

#ifdef NOT_YET
    ASSERT(L_LOCKED(port, L_ENABLE_HFC));
#endif

    dprintf(("enable hfc port 0x%p, enb %d\n", (void *)port, enable));

    if (enable)
	p->ip_sscr |= IOC4_SSCR_HFC_EN;
    else
	p->ip_sscr &= ~IOC4_SSCR_HFC_EN;

    PCI_OUTW(&p->ip_serial->sscr, p->ip_sscr);

    return(0);
}


/*
 * Set external clock
 */
/*ARGSUSED*/
static int
ioc4_set_extclk(sioport_t *port, int clock_factor)
{
#ifdef NOT_YET
    ASSERT(L_LOCKED(port, L_SET_EXTCLK));
    /* XXX still todo */
#endif

    /* only support 0 (no external clock) */
    return(clock_factor);
}


/*
 * Write bytes to the hardware.  Returns the number of bytes
 * actually written.
 */
static int
do_ioc4_write(sioport_t *port, char *buf, int len)
{
    int                prod_ptr, cons_ptr, total;
    struct ring       *outring;
    struct ring_entry *entry;
    ioc4port_t        *p = LPORT(port);
    struct hooks      *hooks = p->ip_hooks;

    DEBUGINC(write_bytes, len);
    DEBUGINC(write_cnt, 1);

    dprintf(("write port 0x%p, len %d\n", (void *)port, len));

    ASSERT(len >= 0);

    prod_ptr = p->ip_tx_prod;
    cons_ptr = PCI_INW(&p->ip_serial->stcir) & PROD_CONS_MASK;
    outring = p->ip_outring;

    /* Maintain a 1-entry red-zone.  The ring buffer is full when
     * (cons - prod) % ring_size is 1.  Rather than do this subtraction
     * in the body of the loop, I'll do it now.
     */
    cons_ptr = (cons_ptr - (int) sizeof(struct ring_entry)) & PROD_CONS_MASK;

    total = 0;
    /* Stuff the bytes into the output */
    while ((prod_ptr != cons_ptr) && (len > 0)) {
	int x;

	/* Go 4 bytes (one ring entry) at a time */
	entry = (struct ring_entry*) ((caddr_t)outring + prod_ptr);

	/* Invalidate all entries */
	entry->ring_allsc = 0;

	/* Copy in some bytes */
	for(x = 0; (x < 4) && (len > 0); x++) {
	    entry->ring_data[x] = *buf++;
	    entry->ring_sc[x] = IOC4_TXCB_VALID;
	    len--;
	    total++;
	}

	DEBUGINC(tx_buf_used, x);
	DEBUGINC(tx_buf_cnt, 1);

	/* If we are within some small threshold of filling up the entire
	 * ring buffer, we must place an EXPLICIT intr here to generate
	 * a lowat interrupt in case we subsequently really do fill up
	 * the ring and the caller goes to sleep.  No need to place
	 * more than one though.
	 */
	if (!(p->ip_flags & LOWAT_WRITTEN) &&
	    ((cons_ptr - prod_ptr) & PROD_CONS_MASK) <=
	    p->ip_tx_lowat * (int)sizeof(struct ring_entry)) {
	    p->ip_flags |= LOWAT_WRITTEN;
	    entry->ring_sc[0] |= IOC4_TXCB_INT_WHEN_DONE;
	    dprintf(("write placing TX_EXPLICIT\n"));
	}

	/* Go on to next entry */
	prod_ptr = (prod_ptr + (int) sizeof(struct ring_entry)) & PROD_CONS_MASK;
    }

    /* If we sent something, start DMA if necessary */
    if (total > 0 && !(p->ip_sscr & IOC4_SSCR_DMA_EN)) {
	p->ip_sscr |= IOC4_SSCR_DMA_EN;
	PCI_OUTW(&p->ip_serial->sscr, p->ip_sscr);
    }

    /* Store the new producer pointer.  If TX is disabled, we stuff the
     * data into the ring buffer, but we don't actually start TX.
     */
    if (!(p->ip_flags & TX_DISABLED)) {
	PCI_OUTW(&p->ip_serial->stpir, prod_ptr);

	/* If we are now transmitting, enable TX_MT interrupt so we
	 * can disable DMA if necessary when the TX finishes.
	 */
	if (total > 0)
	    enable_intrs(p, H_INTR_TX_MT);
    }
    p->ip_tx_prod = prod_ptr;

    dprintf(("write port 0x%p, wrote %d\n", (void *)port, total));
    DEBUGINC(wrote_bytes, total);
    return(total);
}


/* Asynchronous write */
static int
ioc4_write(sioport_t *port, char *buf, int len)
{
#ifdef NOT_YET
    ASSERT(L_LOCKED(port, L_WRITE));
#endif
    return(do_ioc4_write(port, buf, len));
}


/* Synchronous write */
static int
ioc4_sync_write(sioport_t *port, char *buf, int len)
{
    int bytes;

    ASSERT(sio_port_islocked(port));
    bytes = do_ioc4_write(port, buf, len);

    /* Don't allow the system to hang if XOFF is in force */
    if (len > 0 && bytes == 0 && (LPORT(port)->ip_flags & TX_DISABLED))
	ioc4_enable_tx(port, 1);

    return(bytes);
}


/* Write flush */
static void
ioc4_wrflush(sioport_t *port)
{
    ioc4port_t *p = LPORT(port);

    ASSERT(sio_port_islocked(port));

    /* We can't flush if TX is disabled due to XOFF. */
    if (!(PCI_INW(&p->ip_ioc4->sio_ir) & IOC4_SIO_IR_S0_TX_MT) &&
	(p->ip_flags & TX_DISABLED))
	ioc4_enable_tx(port, 1);

    /* Spin waiting for TX_MT to assert only if DMA is enabled.  If we
     * are panicking and one of the other processors is already in
     * symmon, DMA will be disabled and TX_MT will never be asserted.
     * There may also be legitimate cases in the kernel where DMA is
     * disabled and we won't flush correctly here.
     */

    while ((PCI_INW(&p->ip_serial->sscr) & (IOC4_SSCR_DMA_EN |
           IOC4_SSCR_PAUSE_STATE)) == IOC4_SSCR_DMA_EN &&
	   !(PCI_INW(&p->ip_ioc4->sio_ir) & IOC4_SIO_IR_S0_TX_MT)) {
	udelay(5);
    }
}


/*
 * Set or clear break condition on output
 */
static int
ioc4_break(sioport_t *port, int brk)
{
    ioc4port_t *p = LPORT(port);
    char        lcr;
    int         spin_success;

#ifdef NOT_YET
    ASSERT(L_LOCKED(port, L_BREAK));
#endif

    /* Pause the DMA interface if necessary */
    if (p->ip_sscr & IOC4_SSCR_DMA_EN) {
	PCI_OUTW(&p->ip_serial->sscr, p->ip_sscr | IOC4_SSCR_DMA_PAUSE);
	SPIN((PCI_INW(&p->ip_serial->sscr) & IOC4_SSCR_PAUSE_STATE) == 0,
             spin_success);
	if (!spin_success)
		return(-1);
    }

    lcr = PCI_INB(&p->ip_uart->i4u_lcr);
    if (brk) {
	/* Set break */
        PCI_OUTB(&p->ip_uart->i4u_lcr, lcr | LCR_SNDBRK);
    }
    else {
	/* Clear break */
        PCI_OUTB(&p->ip_uart->i4u_lcr, lcr & ~LCR_SNDBRK);
    }

    /* Re-enable the DMA interface if necessary */
    if (p->ip_sscr & IOC4_SSCR_DMA_EN) {
	PCI_OUTW(&p->ip_serial->sscr, p->ip_sscr);
    }

    dprintf(("break port 0x%p, brk %d\n", (void *)port, brk));

    return(0);
}


static int
ioc4_enable_tx(sioport_t *port, int enb)
{
    ioc4port_t   *p = LPORT(port);
    struct hooks *hooks = p->ip_hooks;
    int           spin_success;

#ifdef NOT_YET
    ASSERT(L_LOCKED(port, L_ENABLE_TX));
#endif

    /* If we're already in the desired state, we're done */
    if ((enb && !(p->ip_flags & TX_DISABLED)) ||
	(!enb && (p->ip_flags & TX_DISABLED)))
	return(0);

    /* Pause DMA */
    if (p->ip_sscr & IOC4_SSCR_DMA_EN) {
	PCI_OUTW(&p->ip_serial->sscr, p->ip_sscr | IOC4_SSCR_DMA_PAUSE);
	SPIN((PCI_INW(&p->ip_serial->sscr) & IOC4_SSCR_PAUSE_STATE) == 0,
             spin_success);
	if (!spin_success)
		return(-1);
    }

    if (enb) {
	p->ip_flags &= ~TX_DISABLED;
	PCI_OUTW(&p->ip_serial->stpir, p->ip_tx_prod);
	enable_intrs(p, H_INTR_TX_MT);
    }
    else {
	ioc4reg_t txcons = PCI_INW(&p->ip_serial->stcir) & PROD_CONS_MASK;
	p->ip_flags |= TX_DISABLED;
	disable_intrs(p, H_INTR_TX_MT);

	/* Only move the transmit producer pointer back if the
	 * transmitter is not already empty, otherwise we'll be
	 * generating a bogus entry.
	 */
	if (txcons != p->ip_tx_prod)
	    PCI_OUTW(&p->ip_serial->stpir,
		     (txcons + (int) sizeof(struct ring_entry)) & PROD_CONS_MASK);
    }

    /* Re-enable the DMA interface if necessary */
    if (p->ip_sscr & IOC4_SSCR_DMA_EN) 
	PCI_OUTW(&p->ip_serial->sscr, p->ip_sscr);

    return(0);
}


/*
 * Read in bytes from the hardware.  Return the number of bytes
 * actually read.
 */
static int
ioc4_read(sioport_t *port, char *buf, int len)
{
    int           prod_ptr, cons_ptr, total, x, spin_success;
    struct ring  *inring;
    ioc4port_t   *p = LPORT(port);
    struct hooks *hooks = p->ip_hooks;

#ifdef NOT_YET
    ASSERT(L_LOCKED(port, L_READ));
#endif

    dprintf(("read port 0x%p, len %d\n", (void *)port, len));

    DEBUGINC(read_bytes, len);
    DEBUGINC(read_cnt, 1);

    ASSERT(len >= 0);

    /* There is a nasty timing issue in the IOC4. When the RX_TIMER
     * expires or the RX_HIGH condition arises, we take an interrupt.
     * At some point while servicing the interrupt, we read bytes from
     * the ring buffer and re-arm the RX_TIMER.  However the RX_TIMER is
     * not started until the first byte is received *after* it is armed,
     * and any bytes pending in the RX construction buffers are not drained
     * to memory until either there are 4 bytes available or the RX_TIMER
     * expires.  This leads to a potential situation where data is left
     * in the construction buffers forever because 1 to 3 bytes were received
     * after the interrupt was generated but before the RX_TIMER was re-armed.
     * At that point as long as no subsequent bytes are received the
     * timer will never be started and the bytes will remain in the
     * construction buffer forever.  The solution is to execute a DRAIN
     * command after rearming the timer.  This way any bytes received before
     * the DRAIN will be drained to memory, and any bytes received after
     * the DRAIN will start the TIMER and be drained when it expires.
     * Luckily, this only needs to be done when the DMA buffer is empty
     * since there is no requirement that this function return all
     * available data as long as it returns some.
     */
    /* Re-arm the timer */
    PCI_OUTW(&p->ip_serial->srcir, p->ip_rx_cons | IOC4_SRCIR_ARM);

    prod_ptr = PCI_INW(&p->ip_serial->srpir) & PROD_CONS_MASK;
    cons_ptr = p->ip_rx_cons;

    if (prod_ptr == cons_ptr) {
	int reset_dma = 0;

	/* Input buffer appears empty, do a flush. */

	/* DMA must be enabled for this to work. */
	if (!(p->ip_sscr & IOC4_SSCR_DMA_EN)) {
	    p->ip_sscr |= IOC4_SSCR_DMA_EN;
	    reset_dma = 1;
	}

	/* Potential race condition: we must reload the srpir after
	 * issuing the drain command, otherwise we could think the RX
	 * buffer is empty, then take a very long interrupt, and when
	 * we come back it's full and we wait forever for the drain to
	 * complete.
	 */
	PCI_OUTW(&p->ip_serial->sscr, p->ip_sscr | IOC4_SSCR_RX_DRAIN);
	prod_ptr = PCI_INW(&p->ip_serial->srpir) & PROD_CONS_MASK;

	DEBUGINC(drain, 1);

	/* We must not wait for the DRAIN to complete unless there are
	 * at least 8 bytes (2 ring entries) available to receive the data
	 * otherwise the DRAIN will never complete and we'll deadlock here.
	 * In fact, to make things easier, I'll just ignore the flush if
	 * there is any data at all now available.
	 */
	if (prod_ptr == cons_ptr) {
	    DEBUGINC(drainwait, 1);
	    SPIN(PCI_INW(&p->ip_serial->sscr) & IOC4_SSCR_RX_DRAIN, spin_success);
	    if (!spin_success)
		    return(-1);

	    /* SIGH. We have to reload the prod_ptr *again* since
	     * the drain may have caused it to change
	     */
	    prod_ptr = PCI_INW(&p->ip_serial->srpir) & PROD_CONS_MASK;
	}

	if (reset_dma) {
	    DEBUGINC(resetdma, 1);
	    p->ip_sscr &= ~IOC4_SSCR_DMA_EN;
	    PCI_OUTW(&p->ip_serial->sscr, p->ip_sscr);
	}
    }
    inring = p->ip_inring;

    p->ip_flags &= ~READ_ABORTED;

    total = 0;
    /* Grab bytes from the hardware */
    while(prod_ptr != cons_ptr && len > 0) {
	struct ring_entry *entry;

	entry = (struct ring_entry *) ((caddr_t)inring + cons_ptr);

	/* According to the producer pointer, this ring entry
	 * must contain some data.  But if the PIO happened faster
	 * than the DMA, the data may not be available yet, so let's
	 * wait until it arrives.
	 */
	if ((((volatile struct ring_entry *) entry)->ring_allsc &
	     RING_ANY_VALID) == 0) {

	    /* Indicate the read is aborted so we don't disable
	     * the interrupt thinking that the consumer is
	     * congested.
	     */
	    p->ip_flags |= READ_ABORTED;
		
	    DEBUGINC(read_aborted, 1);
	    len = 0;
	    break;

	}

	/* Load the bytes/status out of the ring entry */
	for(x = 0; x < 4 && len > 0; x++) {
	    char *sc = &(entry->ring_sc[x]);

	    /* Check for change in modem state or overrun */
	    if (*sc & IOC4_RXSB_MODEM_VALID) {
		if (p->ip_notify & N_DDCD) {

		    /* Notify upper layer if DCD dropped */
		    if ((p->ip_flags & DCD_ON) && !(*sc & IOC4_RXSB_DCD)) {

			/* If we have already copied some data, return
			 * it.  We'll pick up the carrier drop on the next
			 * pass.  That way we don't throw away the data
			 * that has already been copied back to the caller's
			 * buffer.
			 */
			if (total > 0) {
			    len = 0;
			    break;
			}

			p->ip_flags &= ~DCD_ON;

			/* Turn off this notification so the carrier
			 * drop protocol won't see it again when it
			 * does a read.
			 */
			*sc &= ~IOC4_RXSB_MODEM_VALID;

			/* To keep things consistent, we need to update
			 * the consumer pointer so the next reader won't
			 * come in and try to read the same ring entries
			 * again.  This must be done here before the call
			 * to UP_DDCD since UP_DDCD may do a recursive
			 * read!
			 */
			if ((entry->ring_allsc & RING_ANY_VALID) == 0)
			    cons_ptr =
				(cons_ptr + (int) sizeof(struct ring_entry)) &
				    PROD_CONS_MASK;

			PCI_OUTW(&p->ip_serial->srcir, cons_ptr);
			p->ip_rx_cons = cons_ptr;

			/* Notify upper layer of carrier drop */
			if (p->ip_notify & N_DDCD)
			    UP_DDCD(port, 0);

			DEBUGINC(read_ddcd, 1);

			/* If we had any data to return, we would have
			 * returned it above.
			 */
			return(0);
		    }
		}

		/* Notify upper layer that an input overrun occurred */
		if ((*sc & IOC4_RXSB_OVERRUN) && (p->ip_notify & N_OVERRUN_ERROR)) {
		    DEBUGINC(rx_overrun, 1);
		    UP_NCS(port, NCS_OVERRUN);
		}

		/* Don't look at this byte again */
		*sc &= ~IOC4_RXSB_MODEM_VALID;
	    }

	    /* Check for valid data or RX errors */
	    if (*sc & IOC4_RXSB_DATA_VALID) {
		if ((*sc & (IOC4_RXSB_PAR_ERR | IOC4_RXSB_FRAME_ERR |
                            IOC4_RXSB_BREAK)) &&
		    (p->ip_notify & (N_PARITY_ERROR | N_FRAMING_ERROR | N_BREAK))) {

		    /* There is an error condition on the next byte.  If
		     * we have already transferred some bytes, we'll stop
		     * here.  Otherwise if this is the first byte to be read,
		     * we'll just transfer it alone after notifying the
		     * upper layer of its status.
		     */
		    if (total > 0) {
			len = 0;
			break;
		    }
		    else {
			if ((*sc & IOC4_RXSB_PAR_ERR) &&
			    (p->ip_notify & N_PARITY_ERROR)) {
			    DEBUGINC(parity, 1);
			    UP_NCS(port, NCS_PARITY);
			}

			if ((*sc & IOC4_RXSB_FRAME_ERR) &&
			    (p->ip_notify & N_FRAMING_ERROR)) {
			    DEBUGINC(framing, 1);
			    UP_NCS(port, NCS_FRAMING);
			}

			if ((*sc & IOC4_RXSB_BREAK) &&
			    (p->ip_notify & N_BREAK)) {
			    DEBUGINC(brk, 1);
			    UP_NCS(port, NCS_BREAK);
			}
			len = 1;
		    }
		}

		*sc &= ~IOC4_RXSB_DATA_VALID;
		*buf++ = entry->ring_data[x];
		len--;
		total++;
	    }
	}

	DEBUGINC(rx_buf_used, x);
	DEBUGINC(rx_buf_cnt, 1);

	/* If we used up this entry entirely, go on to the next one,
	 * otherwise we must have run out of buffer space, so
	 * leave the consumer pointer here for the next read in case
	 * there are still unread bytes in this entry.
	 */
	if ((entry->ring_allsc & RING_ANY_VALID) == 0)
	    cons_ptr = (cons_ptr + (int) sizeof(struct ring_entry)) &
		PROD_CONS_MASK;
    }

    /* Update consumer pointer and re-arm RX timer interrupt */
    PCI_OUTW(&p->ip_serial->srcir, cons_ptr);
    p->ip_rx_cons = cons_ptr;

    /* If we have now dipped below the RX high water mark and we have
     * RX_HIGH interrupt turned off, we can now turn it back on again.
     */
    if ((p->ip_flags & INPUT_HIGH) &&
	(((prod_ptr - cons_ptr) & PROD_CONS_MASK) <
	 ((p->ip_sscr & IOC4_SSCR_RX_THRESHOLD) << IOC4_PROD_CONS_PTR_OFF))) {
	p->ip_flags &= ~INPUT_HIGH;
	enable_intrs(p, H_INTR_RX_HIGH);
    }

    DEBUGINC(red_bytes, total);

    return(total);
}


/*
 * Modify event notification
 */
static int
ioc4_notification(sioport_t *port, int mask, int on)
{
    ioc4port_t   *p = LPORT(port);
    struct hooks *hooks = p->ip_hooks;
    ioc4reg_t     intrbits, sscrbits;

#ifdef NOT_YET
    ASSERT(L_LOCKED(port, L_NOTIFICATION));
#endif
    ASSERT(mask);

    intrbits = sscrbits = 0;

    if (mask & N_DATA_READY)
	intrbits |= (H_INTR_RX_TIMER | H_INTR_RX_HIGH);
    if (mask & N_OUTPUT_LOWAT)
	intrbits |= H_INTR_TX_EXPLICIT;
    if (mask & N_DDCD) {
	intrbits |= H_INTR_DELTA_DCD;
	sscrbits |= IOC4_SSCR_RX_RING_DCD;
    }
    if (mask & N_DCTS)
	intrbits |= H_INTR_DELTA_CTS;

    if (on) {
	enable_intrs(p, intrbits);
	p->ip_notify |= mask;
	p->ip_sscr |= sscrbits;
    }
    else {
	disable_intrs(p, intrbits);
	p->ip_notify &= ~mask;
	p->ip_sscr &= ~sscrbits;
    }

    /* We require DMA if either DATA_READY or DDCD notification is
     * currently requested.  If neither of these is requested and
     * there is currently no TX in progress, DMA may be disabled.
     */
    if (p->ip_notify & (N_DATA_READY | N_DDCD)) 
	p->ip_sscr |= IOC4_SSCR_DMA_EN;
    else if (!(p->ip_ienb & H_INTR_TX_MT)) 
	p->ip_sscr &= ~IOC4_SSCR_DMA_EN;

    PCI_OUTW(&p->ip_serial->sscr, p->ip_sscr);
    return(0);
}


/*
 * Set RX timeout and threshold values.  The upper layer passes in a
 * timeout value.  In all cases it would like to be notified at least this
 * often when there are RX chars coming in.  We set the RX timeout and
 * RX threshold (based on baud) to ensure that the upper layer is called
 * at roughly this interval during normal RX.
 * The input timeout value is in ticks.
 */
static int
ioc4_rx_timeout(sioport_t *port, int timeout)
{
    int         threshold;
    ioc4port_t *p = LPORT(port);

#ifdef NOT_YET
    ASSERT(L_LOCKED(port, L_RX_TIMEOUT));
#endif

    p->ip_rx_timeout = timeout;

    /* Timeout is in ticks.  Let's figure out how many chars we
     * can receive at the current baud rate in that interval
     * and set the RX threshold to that amount.  There are 4 chars
     * per ring entry, so we'll divide the number of chars that will
     * arrive in timeout by 4.
     */
    threshold = timeout * p->ip_baud / 10 / HZ / 4;
    if (threshold == 0)
	threshold = 1; /* otherwise we'll intr all the time! */

    if ((unsigned) threshold > (unsigned) IOC4_SSCR_RX_THRESHOLD)
	    return(1);

    p->ip_sscr &= ~IOC4_SSCR_RX_THRESHOLD;
    p->ip_sscr |= threshold;

    PCI_OUTW(&p->ip_serial->sscr, p->ip_sscr);

    /* Now set the RX timeout to the given value */
    timeout = timeout * IOC4_SRTR_HZ / HZ;
    if (timeout > IOC4_SRTR_CNT)
	timeout = IOC4_SRTR_CNT;

    PCI_OUTW(&p->ip_serial->srtr, timeout);

    return(0);
}


static int
set_DTRRTS(sioport_t *port, int val, int mask1, int mask2)
{
    ioc4port_t *p = LPORT(port);
    ioc4reg_t   shadow;
    int         spin_success;
    char        mcr;

    /* XXX need lock for pretty much this entire routine.  Makes
     * me nervous to hold it for so long.  If we crash or hit
     * a breakpoint in here, we're hosed.
     */

    /* Pause the DMA interface if necessary */
    if (p->ip_sscr & IOC4_SSCR_DMA_EN) {
	PCI_OUTW(&p->ip_serial->sscr, p->ip_sscr | IOC4_SSCR_DMA_PAUSE);
	SPIN((PCI_INW(&p->ip_serial->sscr) & IOC4_SSCR_PAUSE_STATE) == 0,
             spin_success);
	if (!spin_success)
		return(-1);
    }

    shadow = PCI_INW(&p->ip_serial->shadow);
    mcr = (shadow & 0xff000000) >> 24;

    /* Set new value */
    if (val) {
	mcr |= mask1;
        shadow |= mask2;
    }
    else {
	mcr &= ~mask1;
        shadow &= ~mask2;
    }

    PCI_OUTB(&p->ip_uart->i4u_mcr, mcr);

    PCI_OUTW(&p->ip_serial->shadow, shadow);

    /* Re-enable the DMA interface if necessary */
    if (p->ip_sscr & IOC4_SSCR_DMA_EN) 
	PCI_OUTW(&p->ip_serial->sscr, p->ip_sscr);

    return(0);
}


static int
ioc4_set_DTR(sioport_t *port, int dtr)
{
#ifdef NOT_YET
    ASSERT(L_LOCKED(port, L_SET_DTR));
#endif

    dprintf(("set dtr port 0x%p, dtr %d\n", (void *)port, dtr));
    return(set_DTRRTS(port, dtr, MCR_DTR, IOC4_SHADOW_DTR));
}


static int
ioc4_set_RTS(sioport_t *port, int rts)
{
#ifdef NOT_YET
    ASSERT(L_LOCKED(port, L_SET_RTS));
#endif

    dprintf(("set rts port 0x%p, rts %d\n", (void *)port, rts));
    return(set_DTRRTS(port, rts, MCR_RTS, IOC4_SHADOW_RTS));
}


static int
ioc4_query_DCD(sioport_t *port)
{
    ioc4port_t *p = LPORT(port);
    ioc4reg_t   shadow;

#ifdef NOT_YET
    ASSERT(L_LOCKED(port, L_QUERY_DCD));
#endif

    dprintf(("get dcd port 0x%p\n", (void *)port));

    shadow = PCI_INW(&p->ip_serial->shadow);

    return(shadow & IOC4_SHADOW_DCD);
}


static int
ioc4_query_CTS(sioport_t *port)
{
    ioc4port_t *p = LPORT(port);
    ioc4reg_t   shadow;

#ifdef NOT_YET
    ASSERT(L_LOCKED(port, L_QUERY_CTS));
#endif

    dprintf(("get cts port 0x%p\n", (void *)port));

    shadow = PCI_INW(&p->ip_serial->shadow);

    return(shadow & IOC4_SHADOW_CTS);
}


static int
ioc4_set_proto(sioport_t *port, enum sio_proto proto)
{
    ioc4port_t   *p = LPORT(port);
    struct hooks *hooks = p->ip_hooks;

#ifdef NOT_YET
    ASSERT(L_LOCKED(port, L_SET_PROTOCOL));
#endif

    switch(proto) {
      case PROTO_RS232:
	/* Clear the appropriate GIO pin */
	PCI_OUTW((&p->ip_ioc4->gppr_0 + H_RS422), 0);
	break;

      case PROTO_RS422:
	/* Set the appropriate GIO pin */
	PCI_OUTW((&p->ip_ioc4->gppr_0 + H_RS422), 1);
	break;

      default:
	return(1);
    }

    return(0);
}


// #define IS_PORT_0(p) ((p)->ip_hooks == &hooks_array[0])

static int
ioc4_get_mapid(sioport_t *port, void *arg)
{
    return(0);
}


static int
ioc4_set_sscr(sioport_t *port, int arg, int flag)
{
    ioc4port_t *p = LPORT(port);

    if ( flag ) {             /* reset arg bits in p->ip_sscr */
	p->ip_sscr &= ~arg;
    } else {                  /* set bits in p->ip_sscr */
	p->ip_sscr |= arg;
    }
    PCI_OUTW(&p->ip_serial->sscr, p->ip_sscr);
    return(p->ip_sscr);
}
