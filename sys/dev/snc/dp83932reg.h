/*	$FreeBSD$	*/
/*	$NecBSD: dp83932reg.h,v 1.2 1999/02/12 05:50:13 kmatsuda Exp $	*/
/*      $NetBSD: if_snreg.h,v 1.4 1997/06/15 20:20:12 scottr Exp $	*/

/*
 * Copyright (c) 1991   Algorithmics Ltd (http://www.algor.co.uk)
 * You may use, copy, and modify this program so long as you retain the
 * copyright line.
 */

/*
 * if_snreg.h -- National Semiconductor DP8393X (SONIC) register defs
 */

/*
 * SONIC registers as seen by the processor
 */
#define	SNCR_CR		0x00	/* Command */
#define	SNCR_DCR		0x01	/* Data Configuration */
#define	SNCR_RCR		0x02	/* Receive Control */
#define	SNCR_TCR		0x03	/* Transmit Control */
#define	SNCR_IMR		0x04	/* Interrupt Mask */
#define	SNCR_ISR		0x05	/* Interrupt Status */
#define	SNCR_UTDA	0x06	/* Upper Transmit Descriptor Address */
#define	SNCR_CTDA	0x07	/* Current Transmit Descriptor Address */
#define	SNCR_TPS		0x08	/* Transmit Packet Size */
#define	SNCR_TFC		0x09	/* Transmit Fragment Count */
#define	SNCR_TSA0	0x0a	/* Transmit Start Address 0 */
#define	SNCR_TSA1	0x0b	/* Transmit Start Address 1 */
#define	SNCR_TFS		0x0c	/* Transmit Fragment Size */
#define	SNCR_URDA	0x0d	/* Upper Receive Descriptor Address */
#define	SNCR_CRDA	0x0e	/* Current Receive Descriptor Address */
#define	SNCR_CRBA0	0x0f	/* Current Receive Buffer Address 0 */
#define	SNCR_CRBA1	0x10	/* Current Receive Buffer Address 1 */
#define	SNCR_RBWC0	0x11	/* Remaining Buffer Word Count 0 */
#define	SNCR_RBWC1	0x12	/* Remaining Buffer Word Count 1 */
#define	SNCR_EOBC	0x13	/* End Of Buffer Word Count */
#define	SNCR_URRA	0x14	/* Upper Receive Resource Address */
#define	SNCR_RSA		0x15	/* Resource Start Address */
#define	SNCR_REA		0x16	/* Resource End Address */
#define	SNCR_RRP		0x17	/* Resource Read Pointer */
#define	SNCR_RWP		0x18	/* Resource Write Pointer */
#define	SNCR_TRBA0	0x19	/* Temporary Receive Buffer Address 0 */
#define	SNCR_TRBA1	0x1a	/* Temporary Receive Buffer Address 1 */
#define	SNCR_TBWC0	0x1b	/* Temporary Buffer Word Count 0 */
#define	SNCR_TBWC1	0x1c	/* Temporary Buffer Word Count 1 */
#define	SNCR_ADDR0	0x1d	/* Address Generator 0 */
#define	SNCR_ADDR1	0x1e	/* Address Generator 1 */
#define	SNCR_LLFA	0x1f	/* Last Link Field Address */
#define	SNCR_TTDA	0x20	/* Temp Transmit Descriptor Address */
#define	SNCR_CEP		0x21	/* CAM Entry Pointer */
#define	SNCR_CAP2	0x22	/* CAM Address Port 2 */
#define	SNCR_CAP1	0x23	/* CAM Address Port 1 */
#define	SNCR_CAP0	0x24	/* CAM Address Port 0 */
#define	SNCR_CE		0x25	/* CAM Enable */
#define	SNCR_CDP		0x26	/* CAM Descriptor Pointer */
#define	SNCR_CDC		0x27	/* CAM Descriptor Count */
#define	SNCR_SR		0x28	/* Silicon Revision */
#define	SNCR_WT0		0x29	/* Watchdog Timer 0 */
#define	SNCR_WT1		0x2a	/* Watchdog Timer 1 */
#define	SNCR_RSC		0x2b	/* Receive Sequence Counter */
#define	SNCR_CRCT	0x2c	/* CRC Error Tally */
#define	SNCR_FAET	0x2d	/* FAE Tally */
#define	SNCR_MPT		0x2e	/* Missed Packet Tally */
#define	SNCR_MDT		0x2f	/* Maximum Deferral Timer */
#define	SNCR_RTC		0x30	/* Receive Test Control */
#define	SNCR_TTC		0x31	/* Transmit Test Control */
#define	SNCR_DTC		0x32	/* DMA Test Control */
#define	SNCR_CC0		0x33	/* CAM Comparison 0 */
#define	SNCR_CC1		0x34	/* CAM Comparison 1 */
#define	SNCR_CC2		0x35	/* CAM Comparison 2 */
#define	SNCR_CM		0x36	/* CAM Match */
#define	SNCR_RES1	0x37	/* reserved */
#define	SNCR_RES2	0x38	/* reserved */
#define	SNCR_RBC		0x39	/* Receiver Byte Count */
#define	SNCR_RES3	0x3a	/* reserved */
#define	SNCR_TBO		0x3b	/* Transmitter Backoff Counter */
#define	SNCR_TRC		0x3c	/* Transmitter Random Counter */
#define	SNCR_TBM		0x3d	/* Transmitter Backoff Mask */
#define	SNCR_RES4	0x3e	/* Reserved */
#define	SNCR_DCR2	0x3f	/* Data Configuration 2 (AVF) */

#define	SNC_NREGS	0x40

/*
 * Register Interpretations
 */

/*
 * The command register is used for issuing commands to the SONIC.
 * With the exception of CR_RST, the bit is reset when the operation
 * completes.
 */
#define CR_LCAM         0x0200  /* load CAM with descriptor at s_cdp */
#define CR_RRRA         0x0100  /* read next RRA descriptor at s_rrp */
#define CR_RST          0x0080  /* software reset */
#define CR_ST           0x0020  /* start timer */
#define CR_STP          0x0010  /* stop timer */
#define CR_RXEN         0x0008  /* receiver enable */
#define CR_RXDIS        0x0004  /* receiver disable */
#define CR_TXP          0x0002  /* transmit packets */
#define CR_HTX          0x0001  /* halt transmission */

/*
 * The data configuration register establishes the SONIC's bus cycle
 * operation.  This register can only be accessed when the SONIC is in
 * reset mode (s_cr.CR_RST is set.)
 */
#define DCR_EXBUS       0x8000  /* extended bus mode (AVF) */
#define DCR_LBR         0x2000  /* latched bus retry */
#define DCR_PO1         0x1000  /* programmable output 1 */
#define DCR_PO0         0x0800  /* programmable output 0 */
#define DCR_STERM       0x0400  /* synchronous termination */
#define DCR_USR1        0x0200  /* reflects USR1 input pin */
#define DCR_USR0        0x0100  /* reflects USR0 input pin */
#define DCR_WC1         0x0080  /* wait state control 1 */
#define DCR_WC0         0x0040  /* wait state control 0 */
#define DCR_DW          0x0020  /* data width select */
#define DCR_BMS         0x0010  /* DMA block mode select */
#define DCR_RFT1        0x0008  /* receive FIFO threshold control 1 */
#define DCR_RFT0        0x0004  /* receive FIFO threshold control 0 */
#define DCR_TFT1        0x0002  /* transmit FIFO threshold control 1 */
#define DCR_TFT0        0x0001  /* transmit FIFO threshold control 0 */

/* data configuration register aliases */
#define DCR_SYNC        DCR_STERM /* synchronous (memory cycle 2 clocks) */
#define DCR_ASYNC       0         /* asynchronous (memory cycle 3 clocks) */

#define DCR_WAIT0       0                 /* 0 wait states added */
#define DCR_WAIT1       DCR_WC0           /* 1 wait state added */
#define DCR_WAIT2       DCR_WC1           /* 2 wait states added */
#define DCR_WAIT3       (DCR_WC1|DCR_WC0) /* 3 wait states added */

#define DCR_DW16        0       /* use 16-bit DMA accesses */
#define DCR_DW32        DCR_DW  /* use 32-bit DMA accesses */

#define DCR_DMAEF       0       /* DMA until TX/RX FIFO has emptied/filled */
#define DCR_DMABLOCK    DCR_BMS /* DMA until RX/TX threshold crossed */

#define DCR_RFT4        0               /* receive threshold 4 bytes */
#define DCR_RFT8        DCR_RFT0        /* receive threshold 8 bytes */
#define DCR_RFT16       DCR_RFT1        /* receive threshold 16 bytes */
#define DCR_RFT24       (DCR_RFT1|DCR_RFT0) /* receive threshold 24 bytes */

#define DCR_TFT8        0               /* transmit threshold 8 bytes */
#define DCR_TFT16       DCR_TFT0        /* transmit threshold 16 bytes */
#define DCR_TFT24       DCR_TFT1        /* transmit threshold 24 bytes */
#define DCR_TFT28       (DCR_TFT1|DCR_TFT0) /* transmit threshold 28 bytes */

/*
 * The receive control register is used to filter incoming packets and
 * provides status information on packets received.
 * The contents of the register are copied into the RXpkt.status field
 * when a packet is received. RCR_MC - RCR_PRX are then reset.
 */
#define RCR_ERR         0x8000  /* accept packets with CRC errors */
#define RCR_RNT         0x4000  /* accept runt (length < 64) packets */
#define RCR_BRD         0x2000  /* accept broadcast packets */
#define RCR_PRO         0x1000  /* accept all physical address packets */
#define RCR_AMC         0x0800  /* accept all multicast packets */
#define RCR_LB1         0x0400  /* loopback control 1 */
#define RCR_LB0         0x0200  /* loopback control 0 */
#define RCR_MC          0x0100  /* multicast packet received */
#define RCR_BC          0x0080  /* broadcast packet received */
#define RCR_LPKT        0x0040  /* last packet in RBA (RBWC < EOBC) */
#define RCR_CRS         0x0020  /* carrier sense activity */
#define RCR_COL         0x0010  /* collision activity */
#define RCR_CRC         0x0008  /* CRC error */
#define RCR_FAE         0x0004  /* frame alignment error */
#define RCR_LBK         0x0002  /* loopback packet received */
#define RCR_PRX         0x0001  /* packet received without errors */

/* receiver control register aliases */
/* the loopback control bits provide the following options */
#define RCR_LBNONE      0               /* no loopback - normal operation */
#define RCR_LBMAC       RCR_LB0         /* MAC loopback */
#define RCR_LBENDEC     RCR_LB1         /* ENDEC loopback */
#define RCR_LBTRANS     (RCR_LB1|RCR_LB0) /* transceiver loopback */

/*
 * The transmit control register controls the SONIC's transmit operations.
 * TCR_PINT - TCR_EXDIS are loaded from the TXpkt.config field at the
 * start of transmission.  TCR_EXD-TCR_PTX are cleared at the beginning
 * of transmission and updated when the transmission is completed.
 */
#define TCR_PINT        0x8000  /* interrupt when transmission starts */
#define TCR_POWC        0x4000  /* program out of window collision timer */
#define TCR_CRCI        0x2000  /* transmit packet without 4 byte FCS */
#define TCR_EXDIS       0x1000  /* disable excessive deferral timer */
#define TCR_EXD         0x0400  /* excessive deferrals occurred (>3.2ms) */
#define TCR_DEF         0x0200  /* deferred transmissions occurred */
#define TCR_NCRS        0x0100  /* carrier not present during transmission */
#define TCR_CRSL        0x0080  /* carrier lost during transmission */
#define TCR_EXC         0x0040  /* excessive collisions (>16) detected */
#define TCR_OWC         0x0020  /* out of window (bad) collision occurred */
#define TCR_PMB         0x0008  /* packet monitored bad - the tansmitted
                                 * packet had a bad source address or CRC */
#define TCR_FU          0x0004  /* FIFO underrun (memory access failed) */
#define TCR_BCM         0x0002  /* byte count mismatch (TXpkt.pkt_size
                                 * != sum(TXpkt.frag_size) */
#define TCR_PTX         0x0001  /* packet transmitted without errors */
#define	TCR_NC		0xf000	/* after transmission, # of colls */

/* transmit control register aliases */
#define TCR_OWCSFD      0        /* start after start of frame delimiter */
#define TCR_OWCPRE      TCR_POWC /* start after first bit of preamble */


/*
 * The interrupt mask register masks the interrupts that
 * are generated from the interrupt status register.
 * All reserved bits should be written with 0.
 */
#define IMR_BREN        0x4000  /* bus retry occurred enable */
#define IMR_HBLEN       0x2000  /* heartbeat lost enable */
#define IMR_LCDEN       0x1000  /* load CAM done interrupt enable */
#define IMR_PINTEN      0x0800  /* programmable interrupt enable */
#define IMR_PRXEN       0x0400  /* packet received enable */
#define IMR_PTXEN       0x0200  /* packet transmitted enable */
#define IMR_TXEREN      0x0100  /* transmit error enable */
#define IMR_TCEN        0x0080  /* timer complete enable */
#define IMR_RDEEN       0x0040  /* receive descriptors exhausted enable */
#define IMR_RBEEN       0x0020  /* receive buffers exhausted enable */
#define IMR_RBAEEN      0x0010  /* receive buffer area exceeded enable */
#define IMR_CRCEN       0x0008  /* CRC tally counter rollover enable */
#define IMR_FAEEN       0x0004  /* FAE tally counter rollover enable */
#define IMR_MPEN        0x0002  /* MP tally counter rollover enable */
#define IMR_RFOEN       0x0001  /* receive FIFO overrun enable */


/*
 * The interrupt status register indicates the source of an interrupt when
 * the INT pin goes active.  The interrupt is acknowledged by writing
 * the appropriate bit(s) in this register.
 */
#define ISR_ALL         0x7fff  /* all interrupts */
#define ISR_BR          0x4000  /* bus retry occurred */
#define ISR_HBL         0x2000  /* CD heartbeat lost */
#define ISR_LCD         0x1000  /* load CAM command has completed */
#define ISR_PINT        0x0800  /* programmed interrupt from TXpkt.config */
#define ISR_PKTRX       0x0400  /* packet received */
#define ISR_TXDN        0x0200  /* no remaining packets to be transmitted */
#define ISR_TXER        0x0100  /* packet transmission caused error */
#define ISR_TC          0x0080  /* timer complete */
#define ISR_RDE         0x0040  /* receive descriptors exhausted */
#define ISR_RBE         0x0020  /* receive buffers exhausted */
#define ISR_RBAE        0x0010  /* receive buffer area exceeded */
#define ISR_CRC         0x0008  /* CRC tally counter rollover */
#define ISR_FAE         0x0004  /* FAE tally counter rollover */
#define ISR_MP          0x0002  /* MP tally counter rollover */
#define ISR_RFO         0x0001  /* receive FIFO overrun */

/*
 * The second data configuration register allows additional user defined
 * pins to be controlled.  These bits are only available if s_dcr.DCR_EXBUS
 * is set.
 */
#define DCR2_EXPO3      0x8000  /* EXUSR3 output */
#define DCR2_EXPO2      0x4000  /* EXUSR2 output */
#define DCR2_EXPO1      0x2000  /* EXUSR1 output */
#define DCR2_EXPO0      0x1000  /* EXUSR0 output */
#define DCR2_HD         0x0800  /* heart beat disable (83934/83936) */
#define DCR2_JD         0x0200  /* TPI jabber timer disable (83934/83936) */
#define DCR2_AUTO       0x0100  /* AUI/TPI auto selection (83934/83936) */
#define DCR2_XWRAP      0x0040  /* TPI transceiver loopback (83934/83936) */
#define DCR2_FD         0x0020  /* full duplex (83936) */
#define DCR2_PHL        0x0010  /* extend HOLD signal by 1/2 clock */
#define DCR2_LRDY       0x0008  /* set latched ready mode */
#define DCR2_PCM        0x0004  /* packet compress on match */
#define DCR2_PCNM       0x0002  /* packet compress on mismatch */
#define DCR2_RJM        0x0001  /* reject on match */
