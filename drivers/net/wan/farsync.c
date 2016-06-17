/*
 *      FarSync X21 driver for Linux (generic HDLC version)
 *
 *      Actually sync driver for X.21, V.35 and V.24 on FarSync T-series cards
 *
 *      Copyright (C) 2001 FarSite Communications Ltd.
 *      www.farsite.co.uk
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *
 *      Author: R.J.Dunlop      <bob.dunlop@farsite.co.uk>
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <asm/uaccess.h>
#include <linux/if.h>
#include <linux/hdlc.h>

#include "farsync.h"


/*
 *      Module info
 */
MODULE_AUTHOR("R.J.Dunlop <bob.dunlop@farsite.co.uk>");
MODULE_DESCRIPTION("FarSync T-Series X21 driver. FarSite Communications Ltd.");
MODULE_LICENSE("GPL");

EXPORT_NO_SYMBOLS;


/*      Driver configuration and global parameters
 *      ==========================================
 */

/*      Number of ports (per card) supported
 */
#define FST_MAX_PORTS           4


/*      PCI vendor and device IDs
 */
#define FSC_PCI_VENDOR_ID       0x1619  /* FarSite Communications Ltd */
#define T2P_PCI_DEVICE_ID       0x0400  /* T2P X21 2 port card */
#define T4P_PCI_DEVICE_ID       0x0440  /* T4P X21 4 port card */


/*      Default parameters for the link
 */
#define FST_TX_QUEUE_LEN        100     /* At 8Mbps a longer queue length is
                                         * useful, the syncppp module forces
                                         * this down assuming a slower line I
                                         * guess.
                                         */
#define FST_MAX_MTU             8000    /* Huge but possible */
#define FST_DEF_MTU             1500    /* Common sane value */

#define FST_TX_TIMEOUT          (2*HZ)


#ifdef ARPHRD_RAWHDLC
#define ARPHRD_MYTYPE   ARPHRD_RAWHDLC  /* Raw frames */
#else
#define ARPHRD_MYTYPE   ARPHRD_HDLC     /* Cisco-HDLC (keepalives etc) */
#endif


/*      Card shared memory layout
 *      =========================
 */
#pragma pack(1)

/*      This information is derived in part from the FarSite FarSync Smc.h
 *      file. Unfortunately various name clashes and the non-portability of the
 *      bit field declarations in that file have meant that I have chosen to
 *      recreate the information here.
 *
 *      The SMC (Shared Memory Configuration) has a version number that is
 *      incremented every time there is a significant change. This number can
 *      be used to check that we have not got out of step with the firmware
 *      contained in the .CDE files.
 */
#define SMC_VERSION 11

#define FST_MEMSIZE 0x100000    /* Size of card memory (1Mb) */

#define SMC_BASE 0x00002000L    /* Base offset of the shared memory window main
                                 * configuration structure */
#define BFM_BASE 0x00010000L    /* Base offset of the shared memory window DMA
                                 * buffers */

#define LEN_TX_BUFFER 8192      /* Size of packet buffers */
#define LEN_RX_BUFFER 8192

#define LEN_SMALL_TX_BUFFER 256 /* Size of obsolete buffs used for DOS diags */
#define LEN_SMALL_RX_BUFFER 256

#define NUM_TX_BUFFER 2         /* Must be power of 2. Fixed by firmware */
#define NUM_RX_BUFFER 8

/* Interrupt retry time in milliseconds */
#define INT_RETRY_TIME 2


/*      The Am186CH/CC processors support a SmartDMA mode using circular pools
 *      of buffer descriptors. The structure is almost identical to that used
 *      in the LANCE Ethernet controllers. Details available as PDF from the
 *      AMD web site: http://www.amd.com/products/epd/processors/\
 *                    2.16bitcont/3.am186cxfa/a21914/21914.pdf
 */
struct txdesc {                 /* Transmit descriptor */
        volatile u16 ladr;      /* Low order address of packet. This is a
                                 * linear address in the Am186 memory space
                                 */
        volatile u8  hadr;      /* High order address. Low 4 bits only, high 4
                                 * bits must be zero
                                 */
        volatile u8  bits;      /* Status and config */
        volatile u16 bcnt;      /* 2s complement of packet size in low 15 bits.
                                 * Transmit terminal count interrupt enable in
                                 * top bit.
                                 */
                 u16 unused;    /* Not used in Tx */
};

struct rxdesc {                 /* Receive descriptor */
        volatile u16 ladr;      /* Low order address of packet */
        volatile u8  hadr;      /* High order address */
        volatile u8  bits;      /* Status and config */
        volatile u16 bcnt;      /* 2s complement of buffer size in low 15 bits.
                                 * Receive terminal count interrupt enable in
                                 * top bit.
                                 */
        volatile u16 mcnt;      /* Message byte count (15 bits) */
};

/* Convert a length into the 15 bit 2's complement */
/* #define cnv_bcnt(len)   (( ~(len) + 1 ) & 0x7FFF ) */
/* Since we need to set the high bit to enable the completion interrupt this
 * can be made a lot simpler
 */
#define cnv_bcnt(len)   (-(len))

/* Status and config bits for the above */
#define DMA_OWN         0x80            /* SmartDMA owns the descriptor */
#define TX_STP          0x02            /* Tx: start of packet */
#define TX_ENP          0x01            /* Tx: end of packet */
#define RX_ERR          0x40            /* Rx: error (OR of next 4 bits) */
#define RX_FRAM         0x20            /* Rx: framing error */
#define RX_OFLO         0x10            /* Rx: overflow error */
#define RX_CRC          0x08            /* Rx: CRC error */
#define RX_HBUF         0x04            /* Rx: buffer error */
#define RX_STP          0x02            /* Rx: start of packet */
#define RX_ENP          0x01            /* Rx: end of packet */


/* Interrupts from the card are caused by various events and these are presented
 * in a circular buffer as several events may be processed on one physical int
 */
#define MAX_CIRBUFF     32

struct cirbuff {
        u8 rdindex;             /* read, then increment and wrap */
        u8 wrindex;             /* write, then increment and wrap */
        u8 evntbuff[MAX_CIRBUFF];
};

/* Interrupt event codes.
 * Where appropriate the two low order bits indicate the port number
 */
#define CTLA_CHG        0x18    /* Control signal changed */
#define CTLB_CHG        0x19
#define CTLC_CHG        0x1A
#define CTLD_CHG        0x1B

#define INIT_CPLT       0x20    /* Initialisation complete */
#define INIT_FAIL       0x21    /* Initialisation failed */

#define ABTA_SENT       0x24    /* Abort sent */
#define ABTB_SENT       0x25
#define ABTC_SENT       0x26
#define ABTD_SENT       0x27

#define TXA_UNDF        0x28    /* Transmission underflow */
#define TXB_UNDF        0x29
#define TXC_UNDF        0x2A
#define TXD_UNDF        0x2B


/* Port physical configuration. See farsync.h for field values */
struct port_cfg {
        u16  lineInterface;     /* Physical interface type */
        u8   x25op;             /* Unused at present */
        u8   internalClock;     /* 1 => internal clock, 0 => external */
        u32  lineSpeed;         /* Speed in bps */
};

/* Finally sling all the above together into the shared memory structure.
 * Sorry it's a hodge podge of arrays, structures and unused bits, it's been
 * evolving under NT for some time so I guess we're stuck with it.
 * The structure starts at offset SMC_BASE.
 * See farsync.h for some field values.
 */
struct fst_shared {
                                /* DMA descriptor rings */
        struct rxdesc rxDescrRing[FST_MAX_PORTS][NUM_RX_BUFFER];
        struct txdesc txDescrRing[FST_MAX_PORTS][NUM_TX_BUFFER];

                                /* Obsolete small buffers */
        u8  smallRxBuffer[FST_MAX_PORTS][NUM_RX_BUFFER][LEN_SMALL_RX_BUFFER];
        u8  smallTxBuffer[FST_MAX_PORTS][NUM_TX_BUFFER][LEN_SMALL_TX_BUFFER];

        u8  taskStatus;         /* 0x00 => initialising, 0x01 => running,
                                 * 0xFF => halted
                                 */

        u8  interruptHandshake; /* Set to 0x01 by adapter to signal interrupt,
                                 * set to 0xEE by host to acknowledge interrupt
                                 */

        u16 smcVersion;         /* Must match SMC_VERSION */

        u32 smcFirmwareVersion; /* 0xIIVVRRBB where II = product ID, VV = major
                                 * version, RR = revision and BB = build
                                 */

        u16 txa_done;           /* Obsolete completion flags */
        u16 rxa_done;
        u16 txb_done;
        u16 rxb_done;
        u16 txc_done;
        u16 rxc_done;
        u16 txd_done;
        u16 rxd_done;

        u16 mailbox[4];         /* Diagnostics mailbox. Not used */

        struct cirbuff interruptEvent;  /* interrupt causes */

        u32 v24IpSts[FST_MAX_PORTS];    /* V.24 control input status */
        u32 v24OpSts[FST_MAX_PORTS];    /* V.24 control output status */

        struct port_cfg portConfig[FST_MAX_PORTS];

        u16 clockStatus[FST_MAX_PORTS]; /* lsb: 0=> present, 1=> absent */

        u16 cableStatus;                /* lsb: 0=> present, 1=> absent */

        u16 txDescrIndex[FST_MAX_PORTS]; /* transmit descriptor ring index */
        u16 rxDescrIndex[FST_MAX_PORTS]; /* receive descriptor ring index */

        u16 portMailbox[FST_MAX_PORTS][2];      /* command, modifier */
        u16 cardMailbox[4];                     /* Not used */

                                /* Number of times that card thinks the host has
                                 * missed an interrupt by not acknowledging
                                 * within 2mS (I guess NT has problems)
                                 */
        u32 interruptRetryCount;

                                /* Driver private data used as an ID. We'll not
                                 * use this on Linux I'd rather keep such things
                                 * in main memory rather than on the PCI bus
                                 */
        u32 portHandle[FST_MAX_PORTS];

                                /* Count of Tx underflows for stats */
        u32 transmitBufferUnderflow[FST_MAX_PORTS];

                                /* Debounced V.24 control input status */
        u32 v24DebouncedSts[FST_MAX_PORTS];

                                /* Adapter debounce timers. Don't touch */
        u32 ctsTimer[FST_MAX_PORTS];
        u32 ctsTimerRun[FST_MAX_PORTS];
        u32 dcdTimer[FST_MAX_PORTS];
        u32 dcdTimerRun[FST_MAX_PORTS];

        u32 numberOfPorts;      /* Number of ports detected at startup */

        u16 _reserved[64];

        u16 cardMode;           /* Bit-mask to enable features:
                                 * Bit 0: 1 enables LED identify mode
                                 */

        u16 portScheduleOffset;

        u32 endOfSmcSignature;  /* endOfSmcSignature MUST be the last member of
                                 * the structure and marks the end of the shared
                                 * memory. Adapter code initializes its value as
                                 * END_SIG.
                                 */
};

/* endOfSmcSignature value */
#define END_SIG                 0x12345678

/* Mailbox values. (portMailbox) */
#define NOP             0       /* No operation */
#define ACK             1       /* Positive acknowledgement to PC driver */
#define NAK             2       /* Negative acknowledgement to PC driver */
#define STARTPORT       3       /* Start an HDLC port */
#define STOPPORT        4       /* Stop an HDLC port */
#define ABORTTX         5       /* Abort the transmitter for a port */
#define SETV24O         6       /* Set V24 outputs */


/* Larger buffers are positioned in memory at offset BFM_BASE */
struct buf_window {
        u8 txBuffer[FST_MAX_PORTS][NUM_TX_BUFFER][LEN_TX_BUFFER];
        u8 rxBuffer[FST_MAX_PORTS][NUM_RX_BUFFER][LEN_RX_BUFFER];
};

/* Calculate offset of a buffer object within the shared memory window */
#define BUF_OFFSET(X)   ((unsigned int)&(((struct buf_window *)BFM_BASE)->X))

#pragma pack()


/*      Device driver private information
 *      =================================
 */
/*      Per port (line or channel) information
 */
struct fst_port_info {
        hdlc_device             hdlc;   /* HDLC device struct - must be first */
        struct fst_card_info   *card;   /* Card we're associated with */
        int                     index;  /* Port index on the card */
        int                     hwif;   /* Line hardware (lineInterface copy) */
        int                     run;    /* Port is running */
        int                     rxpos;  /* Next Rx buffer to use */
        int                     txpos;  /* Next Tx buffer to use */
        int                     txipos; /* Next Tx buffer to check for free */
        int                     txcnt;  /* Count of Tx buffers in use */
};

/*      Per card information
 */
struct fst_card_info {
        char          *mem;             /* Card memory mapped to kernel space */
        char          *ctlmem;          /* Control memory for PCI cards */
        unsigned int   phys_mem;        /* Physical memory window address */
        unsigned int   phys_ctlmem;     /* Physical control memory address */
        unsigned int   irq;             /* Interrupt request line number */
        unsigned int   nports;          /* Number of serial ports */
        unsigned int   type;            /* Type index of card */
        unsigned int   state;           /* State of card */
        spinlock_t     card_lock;       /* Lock for SMP access */
        unsigned short pci_conf;        /* PCI card config in I/O space */
                                        /* Per port info */
        struct fst_port_info ports[ FST_MAX_PORTS ];
};

/* Convert an HDLC device pointer into a port info pointer and similar */
#define hdlc_to_port(H) ((struct fst_port_info *)(H))
#define dev_to_port(D)  hdlc_to_port(dev_to_hdlc(D))
#define port_to_dev(P)  hdlc_to_dev(&(P)->hdlc)


/*
 *      Shared memory window access macros
 *
 *      We have a nice memory based structure above, which could be directly
 *      mapped on i386 but might not work on other architectures unless we use
 *      the readb,w,l and writeb,w,l macros. Unfortunately these macros take
 *      physical offsets so we have to convert. The only saving grace is that
 *      this should all collapse back to a simple indirection eventually.
 */
#define WIN_OFFSET(X)   ((long)&(((struct fst_shared *)SMC_BASE)->X))

#define FST_RDB(C,E)    readb ((C)->mem + WIN_OFFSET(E))
#define FST_RDW(C,E)    readw ((C)->mem + WIN_OFFSET(E))
#define FST_RDL(C,E)    readl ((C)->mem + WIN_OFFSET(E))

#define FST_WRB(C,E,B)  writeb ((B), (C)->mem + WIN_OFFSET(E))
#define FST_WRW(C,E,W)  writew ((W), (C)->mem + WIN_OFFSET(E))
#define FST_WRL(C,E,L)  writel ((L), (C)->mem + WIN_OFFSET(E))


/*
 *      Debug support
 */
#if FST_DEBUG

static int fst_debug_mask = { FST_DEBUG };

/* Most common debug activity is to print something if the corresponding bit
 * is set in the debug mask. Note: this uses a non-ANSI extension in GCC to
 * support variable numbers of macro parameters. The inverted if prevents us
 * eating someone else's else clause.
 */
#define dbg(F,fmt,A...) if ( ! ( fst_debug_mask & (F))) \
                                ; \
                        else \
                                printk ( KERN_DEBUG FST_NAME ": " fmt, ## A )

#else
# define dbg(X...)      /* NOP */
#endif


/*      Printing short cuts
 */
#define printk_err(fmt,A...)    printk ( KERN_ERR     FST_NAME ": " fmt, ## A )
#define printk_warn(fmt,A...)   printk ( KERN_WARNING FST_NAME ": " fmt, ## A )
#define printk_info(fmt,A...)   printk ( KERN_INFO    FST_NAME ": " fmt, ## A )


/*
 *      PCI ID lookup table
 */
static struct pci_device_id fst_pci_dev_id[] __devinitdata = {
        { FSC_PCI_VENDOR_ID, T2P_PCI_DEVICE_ID, PCI_ANY_ID, PCI_ANY_ID, 0, 0,
                                        FST_TYPE_T2P },
        { FSC_PCI_VENDOR_ID, T4P_PCI_DEVICE_ID, PCI_ANY_ID, PCI_ANY_ID, 0, 0,
                                        FST_TYPE_T4P },
        { 0, }                          /* End */
};

MODULE_DEVICE_TABLE ( pci, fst_pci_dev_id );


/*      Card control functions
 *      ======================
 */
/*      Place the processor in reset state
 *
 * Used to be a simple write to card control space but a glitch in the latest
 * AMD Am186CH processor means that we now have to do it by asserting and de-
 * asserting the PLX chip PCI Adapter Software Reset. Bit 30 in CNTRL register
 * at offset 0x50.
 */
static inline void
fst_cpureset ( struct fst_card_info *card )
{
        unsigned int regval;

        regval = inl ( card->pci_conf + 0x50 );

        outl ( regval |  0x40000000, card->pci_conf + 0x50 );
        outl ( regval & ~0x40000000, card->pci_conf + 0x50 );
}

/*      Release the processor from reset
 */
static inline void
fst_cpurelease ( struct fst_card_info *card )
{
        (void) readb ( card->ctlmem );
}

/*      Clear the cards interrupt flag
 */
static inline void
fst_clear_intr ( struct fst_card_info *card )
{
        /* Poke the appropriate PLX chip register (same as enabling interrupts)
         */
        outw ( 0x0543, card->pci_conf + 0x4C );
}

/*      Disable card interrupts
 */
static inline void
fst_disable_intr ( struct fst_card_info *card )
{
        outw ( 0x0000, card->pci_conf + 0x4C );
}


/*      Issue a Mailbox command for a port.
 *      Note we issue them on a fire and forget basis, not expecting to see an
 *      error and not waiting for completion.
 */
static void
fst_issue_cmd ( struct fst_port_info *port, unsigned short cmd )
{
        struct fst_card_info *card;
        unsigned short mbval;
        unsigned long flags;
        int safety;

        card = port->card;
        spin_lock_irqsave ( &card->card_lock, flags );
        mbval = FST_RDW ( card, portMailbox[port->index][0]);

        safety = 0;
        /* Wait for any previous command to complete */
        while ( mbval > NAK )
        {
                spin_unlock_irqrestore ( &card->card_lock, flags );
                schedule_timeout ( 1 );
                spin_lock_irqsave ( &card->card_lock, flags );

                if ( ++safety > 1000 )
                {
                        printk_err ("Mailbox safety timeout\n");
                        break;
                }

                mbval = FST_RDW ( card, portMailbox[port->index][0]);
        }
        if ( safety > 0 )
        {
                dbg ( DBG_CMD,"Mailbox clear after %d jiffies\n", safety );
        }
        if ( mbval == NAK )
        {
                dbg ( DBG_CMD,"issue_cmd: previous command was NAK'd\n");
        }

        FST_WRW ( card, portMailbox[port->index][0], cmd );

        if ( cmd == ABORTTX || cmd == STARTPORT )
        {
                port->txpos  = 0;
                port->txipos = 0;
                port->txcnt  = 0;
        }

        spin_unlock_irqrestore ( &card->card_lock, flags );
}


/*      Port output signals control
 */
static inline void
fst_op_raise ( struct fst_port_info *port, unsigned int outputs )
{
        outputs |= FST_RDL ( port->card, v24OpSts[port->index]);
        FST_WRL ( port->card, v24OpSts[port->index], outputs );

        if ( port->run )
                fst_issue_cmd ( port, SETV24O );
}

static inline void
fst_op_lower ( struct fst_port_info *port, unsigned int outputs )
{
        outputs = ~outputs & FST_RDL ( port->card, v24OpSts[port->index]);
        FST_WRL ( port->card, v24OpSts[port->index], outputs );

        if ( port->run )
                fst_issue_cmd ( port, SETV24O );
}


/*
 *      Setup port Rx buffers
 */
static void
fst_rx_config ( struct fst_port_info *port )
{
        int i;
        int pi;
        unsigned int offset;
        unsigned long flags;
        struct fst_card_info *card;

        pi   = port->index;
        card = port->card;
        spin_lock_irqsave ( &card->card_lock, flags );
        for ( i = 0 ; i < NUM_RX_BUFFER ; i++ )
        {
                offset = BUF_OFFSET ( rxBuffer[pi][i][0]);

                FST_WRW ( card, rxDescrRing[pi][i].ladr, (u16) offset );
                FST_WRB ( card, rxDescrRing[pi][i].hadr, (u8)( offset >> 16 ));
                FST_WRW ( card, rxDescrRing[pi][i].bcnt,
                                        cnv_bcnt ( LEN_RX_BUFFER ));
                FST_WRW ( card, rxDescrRing[pi][i].mcnt, 0 );
                FST_WRB ( card, rxDescrRing[pi][i].bits, DMA_OWN );
        }
        port->rxpos  = 0;
        spin_unlock_irqrestore ( &card->card_lock, flags );
}


/*
 *      Setup port Tx buffers
 */
static void
fst_tx_config ( struct fst_port_info *port )
{
        int i;
        int pi;
        unsigned int offset;
        unsigned long flags;
        struct fst_card_info *card;

        pi   = port->index;
        card = port->card;
        spin_lock_irqsave ( &card->card_lock, flags );
        for ( i = 0 ; i < NUM_TX_BUFFER ; i++ )
        {
                offset = BUF_OFFSET ( txBuffer[pi][i][0]);

                FST_WRW ( card, txDescrRing[pi][i].ladr, (u16) offset );
                FST_WRB ( card, txDescrRing[pi][i].hadr, (u8)( offset >> 16 ));
                FST_WRW ( card, txDescrRing[pi][i].bcnt, 0 );
                FST_WRB ( card, txDescrRing[pi][i].bits, 0 );
        }
        port->txpos  = 0;
        port->txipos = 0;
        port->txcnt  = 0;
        spin_unlock_irqrestore ( &card->card_lock, flags );
}


/*      Control signal change interrupt event
 */
static void
fst_intr_ctlchg ( struct fst_card_info *card, struct fst_port_info *port )
{
        int signals;

        signals = FST_RDL ( card, v24DebouncedSts[port->index]);

        if ( signals & (( port->hwif == X21 ) ? IPSTS_INDICATE : IPSTS_DCD ))
        {
                if ( ! netif_carrier_ok ( port_to_dev ( port )))
                {
                        dbg ( DBG_INTR,"DCD active\n");
                        netif_carrier_on ( port_to_dev ( port ));
                }
        }
        else
        {
                if ( netif_carrier_ok ( port_to_dev ( port )))
                {
                        dbg ( DBG_INTR,"DCD lost\n");
                        netif_carrier_off ( port_to_dev ( port ));
                }
        }
}


/*      Rx complete interrupt
 */
static void
fst_intr_rx ( struct fst_card_info *card, struct fst_port_info *port )
{
        unsigned char dmabits;
        int pi;
        int rxp;
        unsigned short len;
        struct sk_buff *skb;
        int i;


        /* Check we have a buffer to process */
        pi  = port->index;
        rxp = port->rxpos;
        dmabits = FST_RDB ( card, rxDescrRing[pi][rxp].bits );
        if ( dmabits & DMA_OWN )
        {
                dbg ( DBG_RX | DBG_INTR,"intr_rx: No buffer port %d pos %d\n",
                                        pi, rxp );
                return;
        }

        /* Get buffer length */
        len = FST_RDW ( card, rxDescrRing[pi][rxp].mcnt );
        /* Discard the CRC */
        len -= 2;

        /* Check buffer length and for other errors. We insist on one packet
         * in one buffer. This simplifies things greatly and since we've
         * allocated 8K it shouldn't be a real world limitation
         */
        dbg ( DBG_RX,"intr_rx: %d,%d: flags %x len %d\n", pi, rxp, dmabits,
                                        len );
        if ( dmabits != ( RX_STP | RX_ENP ) || len > LEN_RX_BUFFER - 2 )
        {
                port->hdlc.stats.rx_errors++;

                /* Update error stats and discard buffer */
                if ( dmabits & RX_OFLO )
                {
                        port->hdlc.stats.rx_fifo_errors++;
                }
                if ( dmabits & RX_CRC )
                {
                        port->hdlc.stats.rx_crc_errors++;
                }
                if ( dmabits & RX_FRAM )
                {
                        port->hdlc.stats.rx_frame_errors++;
                }
                if ( dmabits == ( RX_STP | RX_ENP ))
                {
                        port->hdlc.stats.rx_length_errors++;
                }

                /* Discard buffer descriptors until we see the end of packet
                 * marker
                 */
                i = 0;
                while (( dmabits & ( DMA_OWN | RX_ENP )) == 0 )
                {
                        FST_WRB ( card, rxDescrRing[pi][rxp].bits, DMA_OWN );
                        if ( ++rxp >= NUM_RX_BUFFER )
                                rxp = 0;
                        if ( ++i > NUM_RX_BUFFER )
                        {
                                dbg ( DBG_ASS,"intr_rx: Discarding more bufs"
                                                " than we have\n");
                                break;
                        }
                        dmabits = FST_RDB ( card, rxDescrRing[pi][rxp].bits );
                }

                /* Discard the terminal buffer */
                if ( ! ( dmabits & DMA_OWN ))
                {
                        FST_WRB ( card, rxDescrRing[pi][rxp].bits, DMA_OWN );
                        if ( ++rxp >= NUM_RX_BUFFER )
                                rxp = 0;
                }
                port->rxpos = rxp;
                return;
        }

        /* Allocate SKB */
        if (( skb = dev_alloc_skb ( len )) == NULL )
        {
                dbg ( DBG_RX,"intr_rx: can't allocate buffer\n");

                port->hdlc.stats.rx_dropped++;

                /* Return descriptor to card */
                FST_WRB ( card, rxDescrRing[pi][rxp].bits, DMA_OWN );

                if ( ++rxp >= NUM_RX_BUFFER )
                        port->rxpos = 0;
                else
                        port->rxpos = rxp;
                return;
        }

        memcpy_fromio ( skb_put ( skb, len ),
                                card->mem + BUF_OFFSET ( rxBuffer[pi][rxp][0]),
                                len );

        /* Reset buffer descriptor */
        FST_WRB ( card, rxDescrRing[pi][rxp].bits, DMA_OWN );
        if ( ++rxp >= NUM_RX_BUFFER )
                port->rxpos = 0;
        else
                port->rxpos = rxp;

        /* Update stats */
        port->hdlc.stats.rx_packets++;
        port->hdlc.stats.rx_bytes += len;

        /* Push upstream */
        skb->mac.raw = skb->data;
        skb->dev = hdlc_to_dev ( &port->hdlc );
        skb->protocol = hdlc_type_trans(skb, skb->dev);
        netif_rx ( skb );

        port_to_dev ( port )->last_rx = jiffies;
}


/*
 *      The interrupt service routine
 *      Dev_id is our fst_card_info pointer
 */
static void
fst_intr ( int irq, void *dev_id, struct pt_regs *regs )
{
        struct fst_card_info *card;
        struct fst_port_info *port;
        int rdidx;                      /* Event buffer indices */
        int wridx;
        int event;                      /* Actual event for processing */
        int pi;

        if (( card = dev_id ) == NULL )
        {
                dbg ( DBG_INTR,"intr: spurious %d\n", irq );
                return;
        }

        dbg ( DBG_INTR,"intr: %d %p\n", irq, card );

        spin_lock ( &card->card_lock );

        /* Clear and reprime the interrupt source */
        fst_clear_intr ( card );

        /* Set the software acknowledge */
        FST_WRB ( card, interruptHandshake, 0xEE );

        /* Drain the event queue */
        rdidx = FST_RDB ( card, interruptEvent.rdindex );
        wridx = FST_RDB ( card, interruptEvent.wrindex );
        while ( rdidx != wridx )
        {
                event = FST_RDB ( card, interruptEvent.evntbuff[rdidx]);

                port = &card->ports[event & 0x03];

                dbg ( DBG_INTR,"intr: %x\n", event );

                switch ( event )
                {
                case CTLA_CHG:
                case CTLB_CHG:
                case CTLC_CHG:
                case CTLD_CHG:
                        if ( port->run )
                                fst_intr_ctlchg ( card, port );
                        break;

                case ABTA_SENT:
                case ABTB_SENT:
                case ABTC_SENT:
                case ABTD_SENT:
                        dbg ( DBG_TX,"Abort complete port %d\n", event & 0x03 );
                        break;

                case TXA_UNDF:
                case TXB_UNDF:
                case TXC_UNDF:
                case TXD_UNDF:
                        /* Difficult to see how we'd get this given that we
                         * always load up the entire packet for DMA.
                         */
                        dbg ( DBG_TX,"Tx underflow port %d\n", event & 0x03 );
                        port->hdlc.stats.tx_errors++;
                        port->hdlc.stats.tx_fifo_errors++;
                        break;

                case INIT_CPLT:
                        dbg ( DBG_INIT,"Card init OK intr\n");
                        break;

                case INIT_FAIL:
                        dbg ( DBG_INIT,"Card init FAILED intr\n");
                        card->state = FST_IFAILED;
                        break;

                default:
                        printk_err ("intr: unknown card event code. ignored\n");
                        break;
                }

                /* Bump and wrap the index */
                if ( ++rdidx >= MAX_CIRBUFF )
                        rdidx = 0;
        }
        FST_WRB ( card, interruptEvent.rdindex, rdidx );

        for ( pi = 0, port = card->ports ; pi < card->nports ; pi++, port++ )
        {
                if ( ! port->run )
                        continue;

                /* Check for rx completions */
                while ( ! ( FST_RDB ( card, rxDescrRing[pi][port->rxpos].bits )
                                                                & DMA_OWN ))
                {
                        fst_intr_rx ( card, port );
                }

                /* Check for Tx completions */
                while ( port->txcnt > 0 && ! ( FST_RDB ( card,
                        txDescrRing[pi][port->txipos].bits ) & DMA_OWN ))
                {
                        --port->txcnt;
                        if ( ++port->txipos >= NUM_TX_BUFFER )
                                port->txipos = 0;
                        netif_wake_queue ( port_to_dev ( port ));
                }
        }

        spin_unlock ( &card->card_lock );
}


/*      Check that the shared memory configuration is one that we can handle
 *      and that some basic parameters are correct
 */
static void
check_started_ok ( struct fst_card_info *card )
{
        int i;

        /* Check structure version and end marker */
        if ( FST_RDW ( card, smcVersion ) != SMC_VERSION )
        {
                printk_err ("Bad shared memory version %d expected %d\n",
                                FST_RDW ( card, smcVersion ), SMC_VERSION );
                card->state = FST_BADVERSION;
                return;
        }
        if ( FST_RDL ( card, endOfSmcSignature ) != END_SIG )
        {
                printk_err ("Missing shared memory signature\n");
                card->state = FST_BADVERSION;
                return;
        }
        /* Firmware status flag, 0x00 = initialising, 0x01 = OK, 0xFF = fail */
        if (( i = FST_RDB ( card, taskStatus )) == 0x01 )
        {
                card->state = FST_RUNNING;
        }
        else if ( i == 0xFF )
        {
                printk_err ("Firmware initialisation failed. Card halted\n");
                card->state = FST_HALTED;
                return;
        }
        else if ( i != 0x00 )
        {
                printk_err ("Unknown firmware status 0x%x\n", i );
                card->state = FST_HALTED;
                return;
        }

        /* Finally check the number of ports reported by firmware against the
         * number we assumed at card detection. Should never happen with
         * existing firmware etc so we just report it for the moment.
         */
        if ( FST_RDL ( card, numberOfPorts ) != card->nports )
        {
                printk_warn ("Port count mismatch."
                                " Firmware thinks %d we say %d\n",
                                FST_RDL ( card, numberOfPorts ), card->nports );
        }
}


static int
set_conf_from_info ( struct fst_card_info *card, struct fst_port_info *port,
                struct fstioc_info *info )
{
        int err;

        /* Set things according to the user set valid flags.
         * Several of the old options have been invalidated/replaced by the
         * generic HDLC package.
         */
        err = 0;
        if ( info->valid & FSTVAL_PROTO )
                err = -EINVAL;
        if ( info->valid & FSTVAL_CABLE )
                err = -EINVAL;
        if ( info->valid & FSTVAL_SPEED )
                err = -EINVAL;

        if ( info->valid & FSTVAL_MODE )
                FST_WRW ( card, cardMode, info->cardMode );
#if FST_DEBUG
        if ( info->valid & FSTVAL_DEBUG )
                fst_debug_mask = info->debug;
#endif

        return err;
}

static void
gather_conf_info ( struct fst_card_info *card, struct fst_port_info *port,
                struct fstioc_info *info )
{
        int i;

        memset ( info, 0, sizeof ( struct fstioc_info ));

        i = port->index;
        info->nports = card->nports;
        info->type   = card->type;
        info->state  = card->state;
        info->proto  = FST_GEN_HDLC;
        info->index  = i;
#if FST_DEBUG
        info->debug  = fst_debug_mask;
#endif

        /* Only mark information as valid if card is running.
         * Copy the data anyway in case it is useful for diagnostics
         */
        info->valid
                = (( card->state == FST_RUNNING ) ? FSTVAL_ALL : FSTVAL_CARD )
#if FST_DEBUG
                | FSTVAL_DEBUG
#endif
                ;

        info->lineInterface = FST_RDW ( card, portConfig[i].lineInterface );
        info->internalClock = FST_RDB ( card, portConfig[i].internalClock );
        info->lineSpeed     = FST_RDL ( card, portConfig[i].lineSpeed );
        info->v24IpSts      = FST_RDL ( card, v24IpSts[i] );
        info->v24OpSts      = FST_RDL ( card, v24OpSts[i] );
        info->clockStatus   = FST_RDW ( card, clockStatus[i] );
        info->cableStatus   = FST_RDW ( card, cableStatus );
        info->cardMode      = FST_RDW ( card, cardMode );
        info->smcFirmwareVersion  = FST_RDL ( card, smcFirmwareVersion );
}


static int
fst_set_iface ( struct fst_card_info *card, struct fst_port_info *port,
                struct ifreq *ifr )
{
        sync_serial_settings sync;
        int i;

        if (copy_from_user (&sync, ifr->ifr_settings.ifs_ifsu.sync,
			    sizeof (sync)))
                return -EFAULT;

        if ( sync.loopback )
                return -EINVAL;

        i = port->index;

        switch (ifr->ifr_settings.type)
        {
        case IF_IFACE_V35:
                FST_WRW ( card, portConfig[i].lineInterface, V35 );
                port->hwif = V35;
                break;

        case IF_IFACE_V24:
                FST_WRW ( card, portConfig[i].lineInterface, V24 );
                port->hwif = V24;
                break;

        case IF_IFACE_X21:
                FST_WRW ( card, portConfig[i].lineInterface, X21 );
                port->hwif = X21;
                break;

        case IF_IFACE_SYNC_SERIAL:
                break;

        default:
                return -EINVAL;
        }

        switch ( sync.clock_type )
        {
        case CLOCK_EXT:
                FST_WRB ( card, portConfig[i].internalClock, EXTCLK );
                break;

        case CLOCK_INT:
                FST_WRB ( card, portConfig[i].internalClock, INTCLK );
                break;

        default:
                return -EINVAL;
        }
        FST_WRL ( card, portConfig[i].lineSpeed, sync.clock_rate );
        return 0;
}

static int
fst_get_iface ( struct fst_card_info *card, struct fst_port_info *port,
                struct ifreq *ifr )
{
        sync_serial_settings sync;
        int i;

        /* First check what line type is set, we'll default to reporting X.21
         * if nothing is set as IF_IFACE_SYNC_SERIAL implies it can't be
         * changed
         */
        switch ( port->hwif )
        {
        case V35:
                ifr->ifr_settings.type = IF_IFACE_V35;
                break;
        case V24:
                ifr->ifr_settings.type = IF_IFACE_V24;
                break;
        case X21:
        default:
                ifr->ifr_settings.type = IF_IFACE_X21;
                break;
        }

	if (ifr->ifr_settings.size < sizeof(sync)) {
		ifr->ifr_settings.size = sizeof(sync); /* data size wanted */
		return -ENOBUFS;
	}

        i = port->index;
        sync.clock_rate = FST_RDL ( card, portConfig[i].lineSpeed );
        /* Lucky card and linux use same encoding here */
        sync.clock_type = FST_RDB ( card, portConfig[i].internalClock );
        sync.loopback = 0;

        if (copy_to_user (ifr->ifr_settings.ifs_ifsu.sync, &sync,
			  sizeof(sync)))
                return -EFAULT;

        return 0;
}


static int
fst_ioctl ( struct net_device *dev, struct ifreq *ifr, int cmd )
{
        struct fst_card_info *card;
        struct fst_port_info *port;
        struct fstioc_write wrthdr;
        struct fstioc_info info;
        unsigned long flags;

        dbg ( DBG_IOCTL,"ioctl: %x, %p\n", cmd, ifr->ifr_data );

        port = dev_to_port ( dev );
        card = port->card;

        if ( !capable ( CAP_NET_ADMIN ))
                return -EPERM;

        switch ( cmd )
        {
        case FSTCPURESET:
                fst_cpureset ( card );
                card->state = FST_RESET;
                return 0;

        case FSTCPURELEASE:
                fst_cpurelease ( card );
                card->state = FST_STARTING;
                return 0;

        case FSTWRITE:                  /* Code write (download) */

                /* First copy in the header with the length and offset of data
                 * to write
                 */
                if ( ifr->ifr_data == NULL )
                {
                        return -EINVAL;
                }
                if ( copy_from_user ( &wrthdr, ifr->ifr_data,
                                        sizeof ( struct fstioc_write )))
                {
                        return -EFAULT;
                }

                /* Sanity check the parameters. We don't support partial writes
                 * when going over the top
                 */
                if ( wrthdr.size > FST_MEMSIZE || wrthdr.offset > FST_MEMSIZE
				|| wrthdr.size + wrthdr.offset > FST_MEMSIZE )
                {
                        return -ENXIO;
                }

                /* Now copy the data to the card.
                 * This will probably break on some architectures.
                 * I'll fix it when I have something to test on.
                 */
                if ( copy_from_user ( card->mem + wrthdr.offset,
                                ifr->ifr_data + sizeof ( struct fstioc_write ),
                                wrthdr.size ))
                {
                        return -EFAULT;
                }

                /* Writes to the memory of a card in the reset state constitute
                 * a download
                 */
                if ( card->state == FST_RESET )
                {
                        card->state = FST_DOWNLOAD;
                }
                return 0;

        case FSTGETCONF:

                /* If card has just been started check the shared memory config
                 * version and marker
                 */
                if ( card->state == FST_STARTING )
                {
                        check_started_ok ( card );

                        /* If everything checked out enable card interrupts */
                        if ( card->state == FST_RUNNING )
                        {
                                spin_lock_irqsave ( &card->card_lock, flags );
                                fst_clear_intr ( card );
                                FST_WRB ( card, interruptHandshake, 0xEE );
                                spin_unlock_irqrestore ( &card->card_lock,
                                                                flags );
                        }
                }

                if ( ifr->ifr_data == NULL )
                {
                        return -EINVAL;
                }

                gather_conf_info ( card, port, &info );

                if ( copy_to_user ( ifr->ifr_data, &info, sizeof ( info )))
                {
                        return -EFAULT;
                }
                return 0;

        case FSTSETCONF:

                /* Most of the setting have been moved to the generic ioctls
                 * this just covers debug and board ident mode now
                 */
                if ( copy_from_user ( &info,  ifr->ifr_data, sizeof ( info )))
                {
                        return -EFAULT;
                }

                return set_conf_from_info ( card, port, &info );

        case SIOCWANDEV:
                switch (ifr->ifr_settings.type)
                {
                case IF_GET_IFACE:
                        return fst_get_iface ( card, port, ifr );

                case IF_IFACE_SYNC_SERIAL:
                case IF_IFACE_V35:
                case IF_IFACE_V24:
                case IF_IFACE_X21:
                        return fst_set_iface ( card, port, ifr );

                default:
                        return hdlc_ioctl ( dev, ifr, cmd );
                }

        default:
                /* Not one of ours. Pass through to HDLC package */
                return hdlc_ioctl ( dev, ifr, cmd );
        }
}


static void
fst_openport ( struct fst_port_info *port )
{
        int signals;

        /* Only init things if card is actually running. This allows open to
         * succeed for downloads etc.
         */
        if ( port->card->state == FST_RUNNING )
        {
                if ( port->run )
                {
                        dbg ( DBG_OPEN,"open: found port already running\n");

                        fst_issue_cmd ( port, STOPPORT );
                        port->run = 0;
                }

                fst_rx_config ( port );
                fst_tx_config ( port );
                fst_op_raise ( port, OPSTS_RTS | OPSTS_DTR );

                fst_issue_cmd ( port, STARTPORT );
                port->run = 1;

                signals = FST_RDL ( port->card, v24DebouncedSts[port->index]);
                if ( signals & (( port->hwif == X21 ) ? IPSTS_INDICATE
                                                      : IPSTS_DCD ))
                        netif_carrier_on ( port_to_dev ( port ));
                else
                        netif_carrier_off ( port_to_dev ( port ));
        }
}

static void
fst_closeport ( struct fst_port_info *port )
{
        if ( port->card->state == FST_RUNNING )
        {
                if ( port->run )
                {
                        port->run = 0;
                        fst_op_lower ( port, OPSTS_RTS | OPSTS_DTR );

                        fst_issue_cmd ( port, STOPPORT );
                }
                else
                {
                        dbg ( DBG_OPEN,"close: port not running\n");
                }
        }
}


static int
fst_open ( struct net_device *dev )
{
        int err;

        err = hdlc_open ( dev_to_hdlc ( dev ));
        if ( err )
                return err;

        MOD_INC_USE_COUNT;

        fst_openport ( dev_to_port ( dev ));
        netif_wake_queue ( dev );
        return 0;
}

static int
fst_close ( struct net_device *dev )
{
        netif_stop_queue ( dev );
        fst_closeport ( dev_to_port ( dev ));
        hdlc_close ( dev_to_hdlc  ( dev ));
        MOD_DEC_USE_COUNT;
        return 0;
}

static int
fst_attach ( hdlc_device *hdlc, unsigned short encoding, unsigned short parity )
{
        /* Setting currently fixed in FarSync card so we check and forget */
        if ( encoding != ENCODING_NRZ || parity != PARITY_CRC16_PR1_CCITT )
                return -EINVAL;
        return 0;
}


static void
fst_tx_timeout ( struct net_device *dev )
{
        struct fst_port_info *port;

        dbg ( DBG_INTR | DBG_TX,"tx_timeout\n");

        port = dev_to_port ( dev );

        port->hdlc.stats.tx_errors++;
        port->hdlc.stats.tx_aborted_errors++;

        if ( port->txcnt > 0 )
                fst_issue_cmd ( port, ABORTTX );

        dev->trans_start = jiffies;
        netif_wake_queue ( dev );
}


static int
fst_start_xmit ( struct sk_buff *skb, struct net_device *dev )
{
        struct fst_card_info *card;
        struct fst_port_info *port;
        unsigned char dmabits;
        unsigned long flags;
        int pi;
        int txp;

        port = dev_to_port ( dev );
        card = port->card;

        /* Drop packet with error if we don't have carrier */
        if ( ! netif_carrier_ok ( dev ))
        {
                dev_kfree_skb ( skb );
                port->hdlc.stats.tx_errors++;
                port->hdlc.stats.tx_carrier_errors++;
                return 0;
        }

        /* Drop it if it's too big! MTU failure ? */
        if ( skb->len > LEN_TX_BUFFER )
        {
                dbg ( DBG_TX,"Packet too large %d vs %d\n", skb->len,
                                                LEN_TX_BUFFER );
                dev_kfree_skb ( skb );
                port->hdlc.stats.tx_errors++;
                return 0;
        }

        /* Check we have a buffer */
        pi = port->index;
        spin_lock_irqsave ( &card->card_lock, flags );
        txp = port->txpos;
        dmabits = FST_RDB ( card, txDescrRing[pi][txp].bits );
        if ( dmabits & DMA_OWN )
        {
                spin_unlock_irqrestore ( &card->card_lock, flags );
                dbg ( DBG_TX,"Out of Tx buffers\n");
                dev_kfree_skb ( skb );
                port->hdlc.stats.tx_errors++;
                return 0;
        }
        if ( ++port->txpos >= NUM_TX_BUFFER )
                port->txpos = 0;

        if ( ++port->txcnt >= NUM_TX_BUFFER )
                netif_stop_queue ( dev );

        /* Release the card lock before we copy the data as we now have
         * exclusive access to the buffer.
         */
        spin_unlock_irqrestore ( &card->card_lock, flags );

        /* Enqueue the packet */
        memcpy_toio ( card->mem + BUF_OFFSET ( txBuffer[pi][txp][0]),
                                                skb->data, skb->len );
        FST_WRW ( card, txDescrRing[pi][txp].bcnt, cnv_bcnt ( skb->len ));
        FST_WRB ( card, txDescrRing[pi][txp].bits, DMA_OWN | TX_STP | TX_ENP );

        port->hdlc.stats.tx_packets++;
        port->hdlc.stats.tx_bytes += skb->len;

        dev_kfree_skb ( skb );

        dev->trans_start = jiffies;
        return 0;
}


/*
 *      Card setup having checked hardware resources.
 *      Should be pretty bizarre if we get an error here (kernel memory
 *      exhaustion is one possibility). If we do see a problem we report it
 *      via a printk and leave the corresponding interface and all that follow
 *      disabled.
 */
static char *type_strings[] __devinitdata = {
        "no hardware",                  /* Should never be seen */
        "FarSync T2P",
        "FarSync T4P"
};

static void __devinit
fst_init_card ( struct fst_card_info *card )
{
        int i;
        int err;
        struct net_device *dev;

        /* We're working on a number of ports based on the card ID. If the
         * firmware detects something different later (should never happen)
         * we'll have to revise it in some way then.
         */
        for ( i = 0 ; i < card->nports ; i++ )
        {
                card->ports[i].card   = card;
                card->ports[i].index  = i;
                card->ports[i].run    = 0;

                dev = hdlc_to_dev ( &card->ports[i].hdlc );

                /* Fill in the net device info */
                                /* Since this is a PCI setup this is purely
                                 * informational. Give them the buffer addresses
                                 * and basic card I/O.
                                 */
                dev->mem_start   = card->phys_mem
                                 + BUF_OFFSET ( txBuffer[i][0][0]);
                dev->mem_end     = card->phys_mem
                                 + BUF_OFFSET ( txBuffer[i][NUM_TX_BUFFER][0]);
                dev->rmem_start  = card->phys_mem
                                 + BUF_OFFSET ( rxBuffer[i][0][0]);
                dev->rmem_end    = card->phys_mem
                                 + BUF_OFFSET ( rxBuffer[i][NUM_RX_BUFFER][0]);
                dev->base_addr   = card->pci_conf;
                dev->irq         = card->irq;

                dev->tx_queue_len          = FST_TX_QUEUE_LEN;
                dev->open                  = fst_open;
                dev->stop                  = fst_close;
                dev->do_ioctl              = fst_ioctl;
                dev->watchdog_timeo        = FST_TX_TIMEOUT;
                dev->tx_timeout            = fst_tx_timeout;
                card->ports[i].hdlc.attach = fst_attach;
                card->ports[i].hdlc.xmit   = fst_start_xmit;

                if (( err = register_hdlc_device ( &card->ports[i].hdlc )) < 0 )
                {
                        printk_err ("Cannot register HDLC device for port %d"
                                    " (errno %d)\n", i, -err );
                        card->nports = i;
                        break;
                }
        }

        spin_lock_init ( &card->card_lock );

        printk ( KERN_INFO "%s-%s: %s IRQ%d, %d ports\n",
                        hdlc_to_dev(&card->ports[0].hdlc)->name,
                        hdlc_to_dev(&card->ports[card->nports-1].hdlc)->name,
                        type_strings[card->type], card->irq, card->nports );
}


/*
 *      Initialise card when detected.
 *      Returns 0 to indicate success, or errno otherwise.
 */
static int __devinit
fst_add_one ( struct pci_dev *pdev, const struct pci_device_id *ent )
{
        static int firsttime_done = 0;
        struct fst_card_info *card;
        int err = 0;

        if ( ! firsttime_done )
        {
                printk ( KERN_INFO "FarSync X21 driver " FST_USER_VERSION
                                " (c) 2001 FarSite Communications Ltd.\n");
                firsttime_done = 1;
        }

        /* Allocate driver private data */
        card = kmalloc ( sizeof ( struct fst_card_info ),  GFP_KERNEL);
        if (card == NULL)
        {
                printk_err ("FarSync card found but insufficient memory for"
                                " driver storage\n");
                return -ENOMEM;
        }
        memset ( card, 0, sizeof ( struct fst_card_info ));

        /* Record info we need*/
        card->irq         = pdev->irq;
        card->pci_conf    = pci_resource_start ( pdev, 1 );
        card->phys_mem    = pci_resource_start ( pdev, 2 );
        card->phys_ctlmem = pci_resource_start ( pdev, 3 );

        card->type        = ent->driver_data;
        card->nports      = ( ent->driver_data == FST_TYPE_T2P ) ? 2 : 4;

        card->state       = FST_UNINIT;

        dbg ( DBG_PCI,"type %d nports %d irq %d\n", card->type,
                        card->nports, card->irq );
        dbg ( DBG_PCI,"conf %04x mem %08x ctlmem %08x\n",
                        card->pci_conf, card->phys_mem, card->phys_ctlmem );

        /* Check we can get access to the memory and I/O regions */
        if ( ! request_region ( card->pci_conf, 0x80,"PLX config regs"))
        {
                printk_err ("Unable to get config I/O @ 0x%04X\n",
                                                card->pci_conf );
                err = -ENODEV;
                goto error_free_card;
        }
        if ( ! request_mem_region ( card->phys_mem, FST_MEMSIZE,"Shared RAM"))
        {
                printk_err ("Unable to get main memory @ 0x%08X\n",
                                                card->phys_mem );
                err = -ENODEV;
                goto error_release_io;
        }
        if ( ! request_mem_region ( card->phys_ctlmem, 0x10,"Control memory"))
        {
                printk_err ("Unable to get control memory @ 0x%08X\n",
                                                card->phys_ctlmem );
                err = -ENODEV;
                goto error_release_mem;
        }

        /* Try to enable the device */
        if (( err = pci_enable_device ( pdev )) != 0 )
        {
                printk_err ("Failed to enable card. Err %d\n", -err );
                goto error_release_ctlmem;
        }

        /* Get virtual addresses of memory regions */
        if (( card->mem = ioremap ( card->phys_mem, FST_MEMSIZE )) == NULL )
        {
                printk_err ("Physical memory remap failed\n");
                err = -ENODEV;
                goto error_release_ctlmem;
        }
        if (( card->ctlmem = ioremap ( card->phys_ctlmem, 0x10 )) == NULL )
        {
                printk_err ("Control memory remap failed\n");
                err = -ENODEV;
                goto error_unmap_mem;
        }
        dbg ( DBG_PCI,"kernel mem %p, ctlmem %p\n", card->mem, card->ctlmem);

        /* Reset the card's processor */
        fst_cpureset ( card );
        card->state = FST_RESET;

        /* Register the interrupt handler */
        if ( request_irq ( card->irq, fst_intr, SA_SHIRQ, FST_DEV_NAME, card ))
        {

                printk_err ("Unable to register interrupt %d\n", card->irq );
                err = -ENODEV;
                goto error_unmap_ctlmem;
        }

        /* Record driver data for later use */
        pci_set_drvdata(pdev, card);

        /* Remainder of card setup */
        fst_init_card ( card );

        return 0;                       /* Success */


                                        /* Failure. Release resources */
error_unmap_ctlmem:
        iounmap ( card->ctlmem );

error_unmap_mem:
        iounmap ( card->mem );

error_release_ctlmem:
        release_mem_region ( card->phys_ctlmem, 0x10 );

error_release_mem:
        release_mem_region ( card->phys_mem, FST_MEMSIZE );

error_release_io:
        release_region ( card->pci_conf, 0x80 );

error_free_card:
        kfree ( card );
        return err;
}


/*
 *      Cleanup and close down a card
 */
static void __devexit
fst_remove_one ( struct pci_dev *pdev )
{
        struct fst_card_info *card;
        int i;

        card = pci_get_drvdata(pdev);

        for ( i = 0 ; i < card->nports ; i++ )
        {
                unregister_hdlc_device ( &card->ports[i].hdlc );
        }

        fst_disable_intr ( card );
        free_irq ( card->irq, card );

        iounmap ( card->ctlmem );
        iounmap ( card->mem );

        release_mem_region ( card->phys_ctlmem, 0x10 );
        release_mem_region ( card->phys_mem, FST_MEMSIZE );
        release_region ( card->pci_conf, 0x80 );

        kfree ( card );
}

static struct pci_driver fst_driver = {
        name:           FST_NAME,
        id_table:       fst_pci_dev_id,
        probe:          fst_add_one,
        remove:         __devexit_p(fst_remove_one),
        suspend:        NULL,
        resume:         NULL,
};

static int __init
fst_init(void)
{
        return pci_module_init ( &fst_driver );
}

static void __exit
fst_cleanup_module(void)
{
        pci_unregister_driver ( &fst_driver );
}

module_init ( fst_init );
module_exit ( fst_cleanup_module );

