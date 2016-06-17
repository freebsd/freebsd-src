/*
 * SL811HS HCD (Host Controller Driver) for USB.
 * 
 * COPYRIGHT (C) by CYPRESS SEMICONDUCTOR INC 
 * 
 *
 */

#define GET_FRAME_NUMBER(hci)	READ_REG32 (hci, HcFmNumber)

/*
 * Maximum number of root hub ports
 */
#define MAX_ROOT_PORTS		15	/* maximum OHCI root hub ports */

/* control and status registers */
#define HcRevision		0x00
#define HcControl		0x01
#define HcCommandStatus		0x02
#define HcInterruptStatus	0x03
#define HcInterruptEnable	0x04
#define HcInterruptDisable	0x05
#define HcFmInterval		0x0D
#define HcFmRemaining		0x0E
#define HcFmNumber		0x0F
#define HcLSThreshold		0x11
#define HcRhDescriptorA		0x12
#define HcRhDescriptorB		0x13
#define HcRhStatus		0x14
#define HcRhPortStatus		0x15

#define HcHardwareConfiguration 0x20
#define HcDMAConfiguration	0x21
#define HcTransferCounter	0x22
#define HcuPInterrupt		0x24
#define HcuPInterruptEnable	0x25
#define HcChipID		0x27
#define HcScratch		0x28
#define HcSoftwareReset		0x29
#define HcITLBufferLength	0x2A
#define HcATLBufferLength	0x2B
#define HcBufferStatus		0x2C
#define HcReadBackITL0Length	0x2D
#define HcReadBackITL1Length	0x2E
#define HcITLBufferPort		0x40
#define HcATLBufferPort		0x41

/* OHCI CONTROL AND STATUS REGISTER MASKS */

/*
 * HcControl (control) register masks
 */
#define OHCI_CTRL_HCFS		(3 << 6)	/* BUS state mask */
#define OHCI_CTRL_RWC		(1 << 9)	/* remote wakeup connected */
#define OHCI_CTRL_RWE		(1 << 10)	/* remote wakeup enable */

/* pre-shifted values for HCFS */
#define OHCI_USB_RESET		(0 << 6)
#define OHCI_USB_RESUME		(1 << 6)
#define OHCI_USB_OPER		(2 << 6)
#define OHCI_USB_SUSPEND	(3 << 6)

/*
 * HcCommandStatus (cmdstatus) register masks
 */
#define OHCI_HCR	(1 << 0)	/* host controller reset */
#define OHCI_SO		(3 << 16)	/* scheduling overrun count */

/*
 * masks used with interrupt registers:
 * HcInterruptStatus (intrstatus)
 * HcInterruptEnable (intrenable)
 * HcInterruptDisable (intrdisable)
 */
#define OHCI_INTR_SO	(1 << 0)	/* scheduling overrun */

#define OHCI_INTR_SF	(1 << 2)	/* start frame */
#define OHCI_INTR_RD	(1 << 3)	/* resume detect */
#define OHCI_INTR_UE	(1 << 4)	/* unrecoverable error */
#define OHCI_INTR_FNO	(1 << 5)	/* frame number overflow */
#define OHCI_INTR_RHSC	(1 << 6)	/* root hub status change */
#define OHCI_INTR_ATD	(1 << 7)	/* scheduling overrun */

#define OHCI_INTR_MIE	(1 << 31)	/* master interrupt enable */

/*
 * HcHardwareConfiguration
 */
#define InterruptPinEnable	(1 << 0)
#define InterruptPinTrigger	(1 << 1)
#define InterruptOutputPolarity	(1 << 2)
#define DataBusWidth16		(1 << 3)
#define DREQOutputPolarity	(1 << 5)
#define DACKInputPolarity	(1 << 6)
#define EOTInputPolarity	(1 << 7)
#define DACKMode		(1 << 8)
#define AnalogOCEnable		(1 << 10)
#define SuspendClkNotStop	(1 << 11)
#define DownstreamPort15KRSel	(1 << 12)

/* 
 * HcDMAConfiguration
 */
#define DMAReadWriteSelect 	(1 << 0)
#define ITL_ATL_DataSelect	(1 << 1)
#define DMACounterSelect	(1 << 2)
#define DMAEnable		(1 << 4)
#define BurstLen_1		0
#define BurstLen_4		(1 << 5)
#define BurstLen_8		(2 << 5)

/*
 * HcuPInterrupt
 */
#define SOFITLInt		(1 << 0)
#define ATLInt			(1 << 1)
#define AllEOTInterrupt		(1 << 2)
#define OPR_Reg			(1 << 4)
#define HCSuspended		(1 << 5)
#define ClkReady		(1 << 6)

/*
 * HcBufferStatus
 */
#define ITL0BufferFull		(1 << 0)
#define ITL1BufferFull		(1 << 1)
#define ATLBufferFull		(1 << 2)
#define ITL0BufferDone		(1 << 3)
#define ITL1BufferDone		(1 << 4)
#define ATLBufferDone		(1 << 5)

/* OHCI ROOT HUB REGISTER MASKS */

/* roothub.portstatus [i] bits */
#define RH_PS_CCS            0x00000001	/* current connect status */
#define RH_PS_PES            0x00000002	/* port enable status */
#define RH_PS_PSS            0x00000004	/* port suspend status */
#define RH_PS_POCI           0x00000008	/* port over current indicator */
#define RH_PS_PRS            0x00000010	/* port reset status */
#define RH_PS_PPS            0x00000100	/* port power status */
#define RH_PS_LSDA           0x00000200	/* low speed device attached */
#define RH_PS_CSC            0x00010000	/* connect status change */
#define RH_PS_PESC           0x00020000	/* port enable status change */
#define RH_PS_PSSC           0x00040000	/* port suspend status change */
#define RH_PS_OCIC           0x00080000	/* over current indicator change */
#define RH_PS_PRSC           0x00100000	/* port reset status change */

/* roothub.status bits */
#define RH_HS_LPS		0x00000001	/* local power status */
#define RH_HS_OCI		0x00000002	/* over current indicator */
#define RH_HS_DRWE		0x00008000	/* device remote wakeup enable */
#define RH_HS_LPSC		0x00010000	/* local power status change */
#define RH_HS_OCIC		0x00020000	/* over current indicator change */
#define RH_HS_CRWE		0x80000000	/* clear remote wakeup enable */

/* roothub.b masks */
#define RH_B_DR			0x0000ffff	/* device removable flags */
#define RH_B_PPCM		0xffff0000	/* port power control mask */

/* roothub.a masks */
#define	RH_A_NDP		(0xff << 0)	/* number of downstream ports */
#define	RH_A_PSM		(1 << 8)	/* power switching mode */
#define	RH_A_NPS		(1 << 9)	/* no power switching */
#define	RH_A_DT			(1 << 10)	/* device type (mbz) */
#define	RH_A_OCPM		(1 << 11)	/* over current protection mode */
#define	RH_A_NOCP		(1 << 12)	/* no over current protection */
#define	RH_A_POTPGT		(0xff << 24)	/* power on to power good time */

#define URB_DEL 1

#define PORT_STAT_DEFAULT		0x0100
#define PORT_CONNECT_STAT  		0x1
#define PORT_ENABLE_STAT		0x2
#define PORT_SUSPEND_STAT		0x4
#define PORT_OVER_CURRENT_STAT		0x8
#define PORT_RESET_STAT			0x10
#define PORT_POWER_STAT			0x100
#define PORT_LOW_SPEED_DEV_ATTACH_STAT	0x200

#define PORT_CHANGE_DEFAULT		0x0
#define PORT_CONNECT_CHANGE		0x1
#define PORT_ENABLE_CHANGE		0x2
#define PORT_SUSPEND_CHANGE		0x4
#define PORT_OVER_CURRENT_CHANGE	0x8
#define PORT_RESET_CHANGE		0x10

/* Port Status Request info */

typedef struct portstat {
	__u16 portChange;
	__u16 portStatus;
} portstat_t;

typedef struct hcipriv {
	int irq;
	int disabled;		/* e.g. got a UE, we're hung */
	atomic_t resume_count;	/* defending against multiple resumes */
	struct ohci_regs *regs;	/* OHCI controller's memory */
	int hcport;		/* I/O base address */
	int hcport2;		/* I/O data reg addr */

	struct portstat *RHportStatus;	/* root hub port status */

	int intrstatus;
	__u32 hc_control;	/* copy of the hc control reg */

	int frame;

	__u8 *tl;
	int xferPktLen;
	int atl_len;
	int atl_buffer_len;
	int itl0_len;
	int itl1_len;
	int itl_buffer_len;
	int itl_index;
	int tl_last;
	int units_left;

} hcipriv_t;
struct hci;

#define cClt        0		// Control
#define cISO        1		// ISO
#define cBULK       2		// BULK
#define cInt        3		// Interrupt
#define ISO_BIT     0x10

/*-------------------------------------------------------------------------
 * EP0 use for configuration and Vendor Specific command interface
 *------------------------------------------------------------------------*/
#define cMemStart       0x10
#define EP0Buf          0x40	/* SL11H/SL811H memory start at 0x40 */
#define EP0Len          0x40	/* Length of config buffer EP0Buf */
#define EP1Buf          0x60
#define EP1Len          0x40

/*-------------------------------------------------------------------------
 * SL11H/SL811H memory from 80h-ffh use as ping-pong buffer.
 *------------------------------------------------------------------------*/
#define uBufA           0x80	/* buffer A address for DATA0 */
#define uBufB           0xc0	/* buffer B address for DATA1 */
#define uXferLen        0x40	/* xfer length */
#define sMemSize        0xc0	/* Total SL11 memory size */
#define cMemEnd         256

/*-------------------------------------------------------------------------
 * SL811H Register Control memory map
 * --Note: 
 *      --SL11H only has one control register set from 0x00-0x04
 *      --SL811H has two control register set from 0x00-0x04 and 0x08-0x0c
 *------------------------------------------------------------------------*/

#define EP0Control      0x00
#define EP0Address      0x01
#define EP0XferLen      0x02
#define EP0Status       0x03
#define EP0Counter      0x04

#define EP1Control      0x08
#define EP1Address      0x09
#define EP1XferLen      0x0a
#define EP1Status       0x0b
#define EP1Counter      0x0c

#define CtrlReg         0x05
#define IntEna          0x06
			 // 0x07 is reserved
#define IntStatus       0x0d
#define cDATASet        0x0e
#define cSOFcnt         0x0f
#define IntMask         0x57	/* Reset|DMA|EP0|EP2|EP1 for IntEna */
#define HostMask        0x47	/* Host request command  for IntStatus */
#define ReadMask        0xd7	/* Read mask interrupt   for IntStatus */

/*-------------------------------------------------------------------------
 * Standard Chapter 9 definition
 *-------------------------------------------------------------------------
 */
#define GET_STATUS      0x00
#define CLEAR_FEATURE   0x01
#define SET_FEATURE     0x03
#define SET_ADDRESS     0x05
#define GET_DESCRIPTOR  0x06
#define SET_DESCRIPTOR  0x07
#define GET_CONFIG      0x08
#define SET_CONFIG      0x09
#define GET_INTERFACE   0x0a
#define SET_INTERFACE   0x0b
#define SYNCH_FRAME     0x0c

#define DEVICE          0x01
#define CONFIGURATION   0x02
#define STRING          0x03
#define INTERFACE       0x04
#define ENDPOINT        0x05

/*-------------------------------------------------------------------------
 * SL11H/SL811H definition
 *-------------------------------------------------------------------------
 */
#define DATA0_WR	0x07	// (Arm+Enable+tranmist to Host+DATA0)
#define DATA1_WR	0x47	// (Arm+Enable+tranmist to Host on DATA1)
#define ZDATA0_WR	0x05	// (Arm+Transaction Ignored+tranmist to Host+DATA0)
#define ZDATA1_WR	0x45	// (Arm+Transaction Ignored+tranmist to Host+DATA1)
#define DATA0_RD	0x03	// (Arm+Enable+received from Host+DATA0)
#define DATA1_RD	0x43	// (Arm+Enable+received from Host+DATA1)

#define PID_SETUP	0x2d	// USB Specification 1.1 Standard Definition
#define PID_SOF		0xA5
#define PID_IN		0x69
#define PID_OUT		0xe1

#define MAX_RETRY	0xffff
#define TIMEOUT		5		/* 2 mseconds */

#define SL11H_HOSTCTLREG	0
#define SL11H_BUFADDRREG	1
#define SL11H_BUFLNTHREG	2
#define SL11H_PKTSTATREG	3	/* read */
#define SL11H_PIDEPREG		3	/* write */
#define SL11H_XFERCNTREG	4	/* read */
#define SL11H_DEVADDRREG	4	/* write */
#define SL11H_CTLREG1		5
#define SL11H_INTENBLREG	6

// You should not use these registers
 #define SL11H_HOSTCTLREG_B	8
// #define SL11H_BUFADDRREG_B	9
 #define SL11H_BUFLNTHREG_B	0x0A
// #define SL11H_PKTSTATREG_B	0x0B	/* read */
 #define SL11H_PIDEPREG_B	0x0B	/* write */
// #define SL11H_XFERCNTREG_B	0x0C	/* read */
 #define SL11H_DEVADDRREG_B	0x0C	/* write */

#define SL11H_INTSTATREG	0x0D	/* write clears bitwise */
#define SL11H_HWREVREG		0x0E	/* read */
#define SL11H_SOFLOWREG		0x0E	/* write */
#define SL11H_SOFTMRREG		0x0F	/* read */
#define SL11H_CTLREG2		0x0F	/* write */
#define SL11H_DATA_START	0x10

/* Host control register bits (addr 0) */
#define SL11H_HCTLMASK_ARM	1
#define SL11H_HCTLMASK_ENBLEP	2
#define SL11H_HCTLMASK_WRITE	4
#define SL11H_HCTLMASK_ISOCH	0x10
#define SL11H_HCTLMASK_AFTERSOF	0x20
#define SL11H_HCTLMASK_SEQ	0x40
#define SL11H_HCTLMASK_PREAMBLE	0x80

/* Packet status register bits (addr 3) */
#define SL11H_STATMASK_ACK	1
#define SL11H_STATMASK_ERROR	2
#define SL11H_STATMASK_TMOUT	4
#define SL11H_STATMASK_SEQ	8
#define SL11H_STATMASK_SETUP	0x10
#define SL11H_STATMASK_OVF	0x20
#define SL11H_STATMASK_NAK	0x40
#define SL11H_STATMASK_STALL	0x80

/* Control register 1 bits (addr 5) */
#define SL11H_CTL1MASK_DSBLSOF	1
#define SL11H_CTL1MASK_NOTXEOF2	4
#define SL11H_CTL1MASK_DSTATE	0x18
#define SL11H_CTL1MASK_NSPD	0x20
#define SL11H_CTL1MASK_SUSPEND	0x40
#define SL11H_CTL1MASK_CLK12	0x80

#define SL11H_CTL1VAL_RESET	8

/* Interrut enable (addr 6) and interrupt status register bits (addr 0xD) */
#define SL11H_INTMASK_XFERDONE	1
#define SL11H_INTMASK_SOFINTR	0x10
#define SL11H_INTMASK_INSRMV	0x20
#define SL11H_INTMASK_USBRESET	0x40
#define SL11H_INTMASK_DSTATE	0x80	/* only in status reg */

/* HW rev and SOF lo register bits (addr 0xE) */
#define SL11H_HWRMASK_HWREV	0xF0

/* SOF counter and control reg 2 (addr 0xF) */
#define SL11H_CTL2MASK_SOFHI	0x3F
#define SL11H_CTL2MASK_DSWAP	0x40
#define SL11H_CTL2MASK_HOSTMODE	0xae

