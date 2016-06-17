/* $Id: dbri.h,v 1.13 2000/10/13 00:34:24 uzi Exp $
 * drivers/sbus/audio/cs4231.h
 *
 * Copyright (C) 1997 Rudolf Koenig (rfkoenig@immd4.informatik.uni-erlangen.de)
 */

#ifndef _DBRI_H_
#define _DBRI_H_

#include <linux/types.h>

/* DBRI main registers */
#define REG0	0x00UL		/* Status and Control */
#define REG1	0x04UL		/* Mode and Interrupt */
#define REG2	0x08UL		/* Parallel IO */
#define REG3	0x0cUL		/* Test */
#define REG8	0x20UL		/* Command Queue Pointer */
#define REG9	0x24UL		/* Interrupt Queue Pointer */

#define DBRI_NO_CMDS	64
#define DBRI_NO_INTS	1	/* Note: the value of this define was
				 * originally 2.  The ringbuffer to store
				 * interrupts in dma is currently broken.
				 * This is a temporary fix until the ringbuffer
				 * is fixed.
				 */
#define DBRI_INT_BLK	64
#define DBRI_NO_DESCS	64

#define DBRI_MM_ONB	1
#define DBRI_MM_SB	2

struct dbri_mem {
	volatile __u32	word1;
	volatile __u32	ba;			/* Transmit/Receive Buffer Address */
	volatile __u32	nda;			/* Next Descriptor Address */
	volatile __u32	word4;
};

#include "cs4215.h"

/* This structure is in a DMA region where it can accessed by both
 * the CPU and the DBRI
 */
struct dbri_dma {
	volatile s32 cmd[DBRI_NO_CMDS];			/* Place for commands */
	volatile s32 intr[DBRI_NO_INTS * DBRI_INT_BLK];	/* Interrupt field */
	struct dbri_mem desc[DBRI_NO_DESCS];		/* Xmit/receive descriptors */
};

#define dbri_dma_off(member, elem)	\
	((u32)(unsigned long)		\
	 (&(((struct dbri_dma *)0)->member[elem])))

enum in_or_out { PIPEinput, PIPEoutput };

enum direction { in, out };

struct dbri_pipe {
        u32 sdp;				/* SDP command word */
	enum direction direction;
        int nextpipe;				/* Next pipe in linked list */
	int prevpipe;
        int cycle;				/* Offset of timeslot (bits) */
        int length;				/* Length of timeslot (bits) */
        int desc;				/* Index of active descriptor*/
	volatile __u32 *recv_fixed_ptr;		/* Ptr to receive fixed data */
};

struct dbri_desc {
        int inuse;				/* Boolean flag */
        int next;				/* Index of next desc, or -1 */
        void *buffer;				/* CPU view of buffer */
	u32 buffer_dvma;			/* Device view */
        unsigned int len;
        void (*output_callback)(void *, int);
        void *output_callback_arg;
        void (*input_callback)(void *, int, unsigned int);
        void *input_callback_arg;
};

/* This structure holds the information for both chips (DBRI & CS4215) */

struct dbri {
	int regs_size, irq;		/* Needed for unload */
	struct sbus_dev *sdev;		/* SBUS device info */

	volatile struct dbri_dma *dma;	/* Pointer to our DMA block */
	u32 dma_dvma;			/* DBRI visible DMA address */

	unsigned long regs;		/* dbri HW regs */
	int dbri_version;		/* 'e' and up is OK */
	int dbri_irqp;			/* intr queue pointer */
	int wait_seen;

	struct dbri_pipe pipes[32];	/* DBRI's 32 data pipes */
	struct dbri_desc descs[DBRI_NO_DESCS];

	int chi_in_pipe;
	int chi_out_pipe;
	int chi_bpf;

	struct cs4215 mm;		/* mmcodec special info */

#if 0
	/* Where to sleep if busy */
	wait_queue_head_t wait, int_wait;
#endif
	struct audio_info perchip_info;

	/* Track ISDN LIU and notify changes */
	int liu_state;
	void (*liu_callback)(void *);
	void *liu_callback_arg;
};

/* DBRI Reg0 - Status Control Register - defines. (Page 17) */
#define D_P		(1<<15)	/* Program command & queue pointer valid */
#define D_G		(1<<14)	/* Allow 4-Word SBus Burst */
#define D_S		(1<<13)	/* Allow 16-Word SBus Burst */
#define D_E		(1<<12)	/* Allow 8-Word SBus Burst */
#define D_X		(1<<7)	/* Sanity Timer Disable */
#define D_T		(1<<6)	/* Permit activation of the TE interface */
#define D_N		(1<<5)	/* Permit activation of the NT interface */
#define D_C		(1<<4)	/* Permit activation of the CHI interface */
#define D_F		(1<<3)	/* Force Sanity Timer Time-Out */
#define D_D		(1<<2)	/* Disable Master Mode */
#define D_H		(1<<1)	/* Halt for Analysis */
#define D_R		(1<<0)	/* Soft Reset */


/* DBRI Reg1 - Mode and Interrupt Register - defines. (Page 18) */
#define D_LITTLE_END	(1<<8)	/* Byte Order */
#define D_BIG_END	(0<<8)	/* Byte Order */
#define D_MRR		(1<<4)	/* Multiple Error Ack on SBus (readonly) */
#define D_MLE		(1<<3)	/* Multiple Late Error on SBus (readonly) */
#define D_LBG		(1<<2)	/* Lost Bus Grant on SBus (readonly) */
#define D_MBE		(1<<1)	/* Burst Error on SBus (readonly) */
#define D_IR		(1<<0)	/* Interrupt Indicator (readonly) */


/* DBRI Reg2 - Parallel IO Register - defines. (Page 18) */
#define D_ENPIO3	(1<<7)	/* Enable Pin 3 */
#define D_ENPIO2	(1<<6)	/* Enable Pin 2 */
#define D_ENPIO1	(1<<5)	/* Enable Pin 1 */
#define D_ENPIO0	(1<<4)	/* Enable Pin 0 */
#define D_ENPIO		(0xf0)	/* Enable all the pins */
#define D_PIO3		(1<<3)	/* Pin 3: 1: Data mode, 0: Ctrl mode */
#define D_PIO2		(1<<2)	/* Pin 2: 1: Onboard PDN */
#define D_PIO1		(1<<1)	/* Pin 1: 0: Reset */
#define D_PIO0		(1<<0)	/* Pin 0: 1: Speakerbox PDN */


/* DBRI Commands (Page 20) */
#define D_WAIT		0x0	/* Stop execution */
#define D_PAUSE		0x1	/* Flush long pipes */
#define D_JUMP		0x2	/* New command queue */
#define D_IIQ		0x3	/* Initialize Interrupt Queue */
#define D_REX		0x4	/* Report command execution via interrupt */
#define D_SDP		0x5	/* Setup Data Pipe */
#define D_CDP		0x6	/* Continue Data Pipe (reread NULL Pointer) */
#define D_DTS		0x7	/* Define Time Slot */
#define D_SSP		0x8	/* Set short Data Pipe */
#define D_CHI		0x9	/* Set CHI Global Mode */
#define D_NT		0xa	/* NT Command */
#define D_TE		0xb	/* TE Command */
#define D_CDEC		0xc	/* Codec setup */
#define D_TEST		0xd	/* No comment */
#define D_CDM		0xe	/* CHI Data mode command */



/* Special bits for some commands */
#define D_PIPE(v)      ((v)<<0)        /* Pipe Nr: 0-15 long, 16-21 short */

/* Setup Data Pipe */
/* IRM */
#define D_SDP_2SAME	(1<<18) /* Report 2nd time in a row value rcvd*/
#define D_SDP_CHANGE	(2<<18) /* Report any changes */
#define D_SDP_EVERY	(3<<18) /* Report any changes */
#define D_SDP_EOL	(1<<17) /* EOL interrupt enable */
#define D_SDP_IDLE	(1<<16) /* HDLC idle interrupt enable */

/* Pipe data MODE */
#define D_SDP_MEM	(0<<13)	/* To/from memory */
#define D_SDP_HDLC	(2<<13)
#define D_SDP_HDLC_D	(3<<13)	/* D Channel (prio control)*/
#define D_SDP_SER	(4<<13)	/* Serial to serial */
#define D_SDP_FIXED	(6<<13)	/* Short only */
#define D_SDP_MODE(v)	((v)&(7<<13))

#define D_SDP_TO_SER	(1<<12)	/* Direction */
#define D_SDP_FROM_SER	(0<<12)	/* Direction */
#define D_SDP_MSB	(1<<11)	/* Bit order within Byte */
#define D_SDP_LSB	(0<<11)	/* Bit order within Byte */
#define D_SDP_P		(1<<10)	/* Pointer Valid */
#define D_SDP_A		(1<<8)	/* Abort */
#define D_SDP_C		(1<<7)	/* Clear */

/* Define Time Slot */
#define D_DTS_VI	(1<<17) /* Valid Input Time-Slot Descriptor */
#define D_DTS_VO	(1<<16) /* Valid Output Time-Slot Descriptor */
#define D_DTS_INS	(1<<15) /* Insert Time Slot */
#define D_DTS_DEL	(0<<15) /* Delete Time Slot */
#define D_DTS_PRVIN(v) ((v)<<10) /* Previous In Pipe */
#define D_DTS_PRVOUT(v)        ((v)<<5)  /* Previous Out Pipe */

/* Time Slot defines */
#define D_TS_LEN(v)	((v)<<24)	/* Number of bits in this time slot */
#define D_TS_CYCLE(v)	((v)<<14)	/* Bit Count at start of TS */
#define D_TS_DI		(1<<13)	/* Data Invert */
#define D_TS_1CHANNEL	(0<<10)	/* Single Channel / Normal mode */
#define D_TS_MONITOR	(2<<10)	/* Monitor pipe */
#define D_TS_NONCONTIG	(3<<10) /* Non contiguous mode */
#define D_TS_ANCHOR	(7<<10) /* Starting short pipes */
#define D_TS_MON(v)    ((v)<<5)        /* Monitor Pipe */
#define D_TS_NEXT(v)   ((v)<<0)        /* Pipe Nr: 0-15 long, 16-21 short */

/* Concentration Highway Interface Modes */
#define D_CHI_CHICM(v)	((v)<<16)	/* Clock mode */
#define D_CHI_IR	(1<<15) /* Immediate Interrupt Report */
#define D_CHI_EN	(1<<14) /* CHIL Interrupt enabled */
#define D_CHI_OD	(1<<13) /* Open Drain Enable */
#define D_CHI_FE	(1<<12) /* Sample CHIFS on Rising Frame Edge */
#define D_CHI_FD	(1<<11) /* Frame Drive */
#define D_CHI_BPF(v)	((v)<<0)	/* Bits per Frame */

/* NT: These are here for completeness */
#define D_NT_FBIT	(1<<17)	/* Frame Bit */
#define D_NT_NBF	(1<<16)	/* Number of bad frames to loose framing */
#define D_NT_IRM_IMM	(1<<15)	/* Interrupt Report & Mask: Immediate */
#define D_NT_IRM_EN	(1<<14)	/* Interrupt Report & Mask: Enable */
#define D_NT_ISNT	(1<<13)	/* Configfure interface as NT */
#define D_NT_FT		(1<<12)	/* Fixed Timing */
#define D_NT_EZ		(1<<11)	/* Echo Channel is Zeros */
#define D_NT_IFA	(1<<10)	/* Inhibit Final Activation */
#define D_NT_ACT	(1<<9)	/* Activate Interface */
#define D_NT_MFE	(1<<8)	/* Multiframe Enable */
#define D_NT_RLB(v)	((v)<<5)	/* Remote Loopback */
#define D_NT_LLB(v)	((v)<<2)	/* Local Loopback */
#define D_NT_FACT	(1<<1)	/* Force Activation */
#define D_NT_ABV	(1<<0)	/* Activate Bipolar Violation */

/* Codec Setup */
#define D_CDEC_CK(v)	((v)<<24)	/* Clock Select */
#define D_CDEC_FED(v)	((v)<<12)	/* FSCOD Falling Edge Delay */
#define D_CDEC_RED(v)	((v)<<0)	/* FSCOD Rising Edge Delay */

/* Test */
#define D_TEST_RAM(v)	((v)<<16)	/* RAM Pointer */
#define D_TEST_SIZE(v)	((v)<<11)	/* */
#define D_TEST_ROMONOFF	0x5	/* Toggle ROM opcode monitor on/off */
#define D_TEST_PROC	0x6	/* MicroProcessor test */
#define D_TEST_SER	0x7	/* Serial-Controller test */
#define D_TEST_RAMREAD	0x8	/* Copy from Ram to system memory */
#define D_TEST_RAMWRITE	0x9	/* Copy into Ram from system memory */
#define D_TEST_RAMBIST	0xa	/* RAM Built-In Self Test */
#define D_TEST_MCBIST	0xb	/* Microcontroller Built-In Self Test */
#define D_TEST_DUMP	0xe	/* ROM Dump */

/* CHI Data Mode */
#define D_CDM_THI	(1<<8)	/* Transmit Data on CHIDR Pin */
#define D_CDM_RHI	(1<<7)	/* Receive Data on CHIDX Pin */
#define D_CDM_RCE	(1<<6)	/* Receive on Rising Edge of CHICK */
#define D_CDM_XCE	(1<<2)	/* Transmit Data on Rising Edge of CHICK */
#define D_CDM_XEN	(1<<1)	/* Transmit Highway Enable */
#define D_CDM_REN	(1<<0)	/* Receive Highway Enable */

/* The Interrupts */
#define D_INTR_BRDY	1	/* Buffer Ready for processing */
#define D_INTR_MINT	2	/* Marked Interrupt in RD/TD */
#define D_INTR_IBEG	3	/* Flag to idle transition detected (HDLC) */
#define D_INTR_IEND	4	/* Idle to flag transition detected (HDLC) */
#define D_INTR_EOL	5	/* End of List */
#define D_INTR_CMDI	6	/* Command has bean read */
#define D_INTR_XCMP	8	/* Transmission of frame complete */
#define D_INTR_SBRI	9	/* BRI status change info */
#define D_INTR_FXDT	10	/* Fixed data change */
#define D_INTR_CHIL	11	/* CHI lost frame sync (channel 36 only) */
#define D_INTR_COLL	11	/* Unrecoverable D-Channel collision */
#define D_INTR_DBYT	12	/* Dropped by frame slip */
#define D_INTR_RBYT	13	/* Repeated by frame slip */
#define D_INTR_LINT	14	/* Lost Interrupt */
#define D_INTR_UNDR	15	/* DMA underrun */

#define D_INTR_TE	32
#define D_INTR_NT	34
#define D_INTR_CHI	36
#define D_INTR_CMD	38

#define D_INTR_GETCHAN(v)	(((v)>>24) & 0x3f)
#define D_INTR_GETCODE(v)	(((v)>>20) & 0xf)
#define D_INTR_GETCMD(v)	(((v)>>16) & 0xf)
#define D_INTR_GETVAL(v)	((v) & 0xffff)
#define D_INTR_GETRVAL(v)	((v) & 0xfffff)

#define D_P_0		0	/* TE receive anchor */
#define D_P_1		1	/* TE transmit anchor */
#define D_P_2		2	/* NT transmit anchor */
#define D_P_3		3	/* NT receive anchor */
#define D_P_4		4	/* CHI send data */
#define D_P_5		5	/* CHI receive data */
#define D_P_6		6	/* */
#define D_P_7		7	/* */
#define D_P_8		8	/* */
#define D_P_9		9	/* */
#define D_P_10		10	/* */
#define D_P_11		11	/* */
#define D_P_12		12	/* */
#define D_P_13		13	/* */
#define D_P_14		14	/* */
#define D_P_15		15	/* */
#define D_P_16		16	/* CHI anchor pipe */
#define D_P_17		17	/* CHI send */
#define D_P_18		18	/* CHI receive */
#define D_P_19		19	/* CHI receive */
#define D_P_20		20	/* CHI receive */
#define D_P_21		21	/* */
#define D_P_22		22	/* */
#define D_P_23		23	/* */
#define D_P_24		24	/* */
#define D_P_25		25	/* */
#define D_P_26		26	/* */
#define D_P_27		27	/* */
#define D_P_28		28	/* */
#define D_P_29		29	/* */
#define D_P_30		30	/* */
#define D_P_31		31	/* */


/* Transmit descriptor defines */
#define DBRI_TD_F	(1<<31)	/* End of Frame */
#define DBRI_TD_D	(1<<30)	/* Do not append CRC */
#define DBRI_TD_CNT(v)	((v)<<16)	/* Number of valid bytes in the buffer */
#define DBRI_TD_B	(1<<15)	/* Final interrupt */
#define DBRI_TD_M	(1<<14)	/* Marker interrupt */
#define DBRI_TD_I	(1<<13)	/* Transmit Idle Characters */
#define DBRI_TD_FCNT(v)	(v)	/* Flag Count */
#define DBRI_TD_UNR	(1<<3)	/* Underrun: transmitter is out of data */
#define DBRI_TD_ABT	(1<<2)	/* Abort: frame aborted */
#define DBRI_TD_TBC	(1<<0)	/* Transmit buffer Complete */
#define DBRI_TD_STATUS(v)       ((v)&0xff)      /* Transmit status */

/* Receive descriptor defines */
#define DBRI_RD_F	(1<<31)	/* End of Frame */
#define DBRI_RD_C	(1<<30)	/* Completed buffer */
#define DBRI_RD_B	(1<<15)	/* Final interrupt */
#define DBRI_RD_M	(1<<14)	/* Marker interrupt */
#define DBRI_RD_BCNT(v)	(v)	/* Buffer size */
#define DBRI_RD_CRC	(1<<7)	/* 0: CRC is correct */
#define DBRI_RD_BBC	(1<<6)	/* 1: Bad Byte received */
#define DBRI_RD_ABT	(1<<5)	/* Abort: frame aborted */
#define DBRI_RD_OVRN	(1<<3)	/* Overrun: data lost */
#define DBRI_RD_STATUS(v)      ((v)&0xff)      /* Receive status */
#define DBRI_RD_CNT(v) (((v)>>16)&0x1fff)        /* Number of valid bytes in the buffer */

#endif /* _DBRI_H_ */
