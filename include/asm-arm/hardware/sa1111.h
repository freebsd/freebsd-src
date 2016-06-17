/*
 * linux/include/asm-arm/hardware/SA-1111.h
 *
 * Copyright (C) 2000 John G Dorsey <john+@cs.cmu.edu>
 *
 * This file contains definitions for the SA-1111 Companion Chip.
 * (Structure and naming borrowed from SA-1101.h, by Peter Danielsson.)
 *
 * Macro that calculates real address for registers in the SA-1111
 */

#ifndef _ASM_ARCH_SA1111
#define _ASM_ARCH_SA1111

#include <asm/arch/bitfield.h>

/*
 * The SA1111 is always located at virtual 0xf4000000, and is always
 * "native" endian.
 */

#define SA1111_VBASE		0xf4000000

/* Don't use these! */
#define SA1111_p2v( x )         ((x) - SA1111_BASE + SA1111_VBASE)
#define SA1111_v2p( x )         ((x) - SA1111_VBASE + SA1111_BASE)

#ifndef __ASSEMBLY__

extern struct resource sa1111_resource;
#define _SA1111(x)	((x) + sa1111_resource.start)
#endif

/*
 * 26 bits of the SA-1110 address bus are available to the SA-1111.
 * Use these when feeding target addresses to the DMA engines.
 */

#define SA1111_ADDR_WIDTH	(26)
#define SA1111_ADDR_MASK	((1<<SA1111_ADDR_WIDTH)-1)
#define SA1111_DMA_ADDR(x)	((x)&SA1111_ADDR_MASK)

/*
 * Don't ask the (SAC) DMA engines to move less than this amount.
 */

#define SA1111_SAC_DMA_MIN_XFER	(0x800)

/*
 * SA1111 register definitions.
 */
#define __CCREG(x)	__REGP(SA1111_VBASE + (x))

/* System Bus Interface (SBI)
 *
 * Registers
 *    SKCR	Control Register
 *    SMCR	Shared Memory Controller Register
 *    SKID	ID Register
 */
#define SA1111_SKCR	0x0000
#define SA1111_SMCR	0x0004
#define SA1111_SKID	0x0008

#define _SBI_SKCR	_SA1111(SA1111_SKCR)
#define _SBI_SMCR	_SA1111(SA1111_SMCR)
#define _SBI_SKID	_SA1111(SA1111_SKID)

#if LANGUAGE == C

#define SBI_SKCR	__CCREG(SA1111_SKCR)
#define SBI_SMCR	__CCREG(SA1111_SMCR)
#define SBI_SKID	__CCREG(SA1111_SKID)

#endif  /* LANGUAGE == C */

#define SKCR_PLL_BYPASS	(1<<0)
#define SKCR_RCLKEN	(1<<1)
#define SKCR_SLEEP	(1<<2)
#define SKCR_DOZE	(1<<3)
#define SKCR_VCO_OFF	(1<<4)
#define SKCR_SCANTSTEN	(1<<5)
#define SKCR_CLKTSTEN	(1<<6)
#define SKCR_RDYEN	(1<<7)
#define SKCR_SELAC	(1<<8)
#define SKCR_OPPC	(1<<9)
#define SKCR_PLLTSTEN	(1<<10)
#define SKCR_USBIOTSTEN	(1<<11)
/*
 * Don't believe the specs!  Take them, throw them outside.  Leave them
 * there for a week.  Spit on them.  Walk on them.  Stamp on them.
 * Pour gasoline over them and finally burn them.  Now think about coding.
 *  - The October 1999 errata (278260-007) says its bit 13, 1 to enable.
 *  - The Feb 2001 errata (278260-010) says that the previous errata
 *    (278260-009) is wrong, and its bit actually 12, fixed in spec
 *    278242-003.
 *  - The SA1111 manual (278242) says bit 12, but 0 to enable.
 *  - Reality is bit 13, 1 to enable.
 *      -- rmk
 */
#define SKCR_OE_EN	(1<<13)

#define SMCR_DTIM	(1<<0)
#define SMCR_MBGE	(1<<1)
#define SMCR_DRAC_0	(1<<2)
#define SMCR_DRAC_1	(1<<3)
#define SMCR_DRAC_2	(1<<4)
#define SMCR_DRAC	Fld(3, 2)
#define SMCR_CLAT	(1<<5)

#define SKID_SIREV_MASK	(0x000000f0)
#define SKID_MTREV_MASK (0x0000000f)
#define SKID_ID_MASK	(0xffffff00)
#define SKID_SA1111_ID	(0x690cc200)

/*
 * System Controller
 *
 * Registers
 *    SKPCR	Power Control Register
 *    SKCDR	Clock Divider Register
 *    SKAUD	Audio Clock Divider Register
 *    SKPMC	PS/2 Mouse Clock Divider Register
 *    SKPTC	PS/2 Track Pad Clock Divider Register
 *    SKPEN0	PWM0 Enable Register
 *    SKPWM0	PWM0 Clock Register
 *    SKPEN1	PWM1 Enable Register
 *    SKPWM1	PWM1 Clock Register
 */

#define _SKPCR		_SA1111(0x0200)
#define _SKCDR		_SA1111(0x0204)
#define _SKAUD		_SA1111(0x0208)
#define _SKPMC		_SA1111(0x020c)
#define _SKPTC		_SA1111(0x0210)
#define _SKPEN0		_SA1111(0x0214)
#define _SKPWM0		_SA1111(0x0218)
#define _SKPEN1		_SA1111(0x021c)
#define _SKPWM1		_SA1111(0x0220)

#if LANGUAGE == C

#define SKPCR		__CCREG(0x0200)
#define SKCDR		__CCREG(0x0204)
#define SKAUD		__CCREG(0x0208)
#define SKPMC		__CCREG(0x020c)
#define SKPTC		__CCREG(0x0210)
#define SKPEN0		__CCREG(0x0214)
#define SKPWM0		__CCREG(0x0218)
#define SKPEN1		__CCREG(0x021c)
#define SKPWM1		__CCREG(0x0220)

#endif  /* LANGUAGE == C */

#define SKPCR_UCLKEN	(1<<0)
#define SKPCR_ACCLKEN	(1<<1)
#define SKPCR_I2SCLKEN	(1<<2)
#define SKPCR_L3CLKEN	(1<<3)
#define SKPCR_SCLKEN	(1<<4)
#define SKPCR_PMCLKEN	(1<<5)
#define SKPCR_PTCLKEN	(1<<6)
#define SKPCR_DCLKEN	(1<<7)
#define SKPCR_PWMCLKEN	(1<<8)

/*
 * USB Host controller
 */
#define _USB_OHCI_OP_BASE	_SA1111( 0x400 )
#define _USB_STATUS		_SA1111( 0x518 )
#define _USB_RESET		_SA1111( 0x51c )
#define _USB_INTERRUPTEST	_SA1111( 0x520 )

#define _USB_EXTENT		(_USB_INTERRUPTEST - _USB_OHCI_OP_BASE + 4)

#if LANGUAGE == C

#define USB_OHCI_OP_BASE	__CCREG(0x0400)
#define USB_STATUS		__CCREG(0x0518)
#define USB_RESET		__CCREG(0x051c)
#define USB_INTERRUPTEST	__CCReG(0x0520)

#endif  /* LANGUAGE == C */

#define USB_RESET_FORCEIFRESET	(1 << 0)
#define USB_RESET_FORCEHCRESET	(1 << 1)
#define USB_RESET_CLKGENRESET	(1 << 2)
#define USB_RESET_SIMSCALEDOWN	(1 << 3)
#define USB_RESET_USBINTTEST	(1 << 4)
#define USB_RESET_SLEEPSTBYEN	(1 << 5)
#define USB_RESET_PWRSENSELOW	(1 << 6)
#define USB_RESET_PWRCTRLLOW	(1 << 7)

/*
 * Serial Audio Controller
 *
 * Registers
 *    SACR0             Serial Audio Common Control Register
 *    SACR1             Serial Audio Alternate Mode (I2C/MSB) Control Register
 *    SACR2             Serial Audio AC-link Control Register
 *    SASR0             Serial Audio I2S/MSB Interface & FIFO Status Register
 *    SASR1             Serial Audio AC-link Interface & FIFO Status Register
 *    SASCR             Serial Audio Status Clear Register
 *    L3_CAR            L3 Control Bus Address Register
 *    L3_CDR            L3 Control Bus Data Register
 *    ACCAR             AC-link Command Address Register
 *    ACCDR             AC-link Command Data Register
 *    ACSAR             AC-link Status Address Register
 *    ACSDR             AC-link Status Data Register
 *    SADTCS            Serial Audio DMA Transmit Control/Status Register
 *    SADTSA            Serial Audio DMA Transmit Buffer Start Address A
 *    SADTCA            Serial Audio DMA Transmit Buffer Count Register A
 *    SADTSB            Serial Audio DMA Transmit Buffer Start Address B
 *    SADTCB            Serial Audio DMA Transmit Buffer Count Register B
 *    SADRCS            Serial Audio DMA Receive Control/Status Register
 *    SADRSA            Serial Audio DMA Receive Buffer Start Address A
 *    SADRCA            Serial Audio DMA Receive Buffer Count Register A
 *    SADRSB            Serial Audio DMA Receive Buffer Start Address B
 *    SADRCB            Serial Audio DMA Receive Buffer Count Register B
 *    SAITR             Serial Audio Interrupt Test Register
 *    SADR              Serial Audio Data Register (16 x 32-bit)
 */

#define _SACR0          _SA1111( 0x0600 )
#define _SACR1          _SA1111( 0x0604 )
#define _SACR2          _SA1111( 0x0608 )
#define _SASR0          _SA1111( 0x060c )
#define _SASR1          _SA1111( 0x0610 )
#define _SASCR          _SA1111( 0x0618 )
#define _L3_CAR         _SA1111( 0x061c )
#define _L3_CDR         _SA1111( 0x0620 )
#define _ACCAR          _SA1111( 0x0624 )
#define _ACCDR          _SA1111( 0x0628 )
#define _ACSAR          _SA1111( 0x062c )
#define _ACSDR          _SA1111( 0x0630 )
#define _SADTCS         _SA1111( 0x0634 )
#define _SADTSA         _SA1111( 0x0638 )
#define _SADTCA         _SA1111( 0x063c )
#define _SADTSB         _SA1111( 0x0640 )
#define _SADTCB         _SA1111( 0x0644 )
#define _SADRCS         _SA1111( 0x0648 )
#define _SADRSA         _SA1111( 0x064c )
#define _SADRCA         _SA1111( 0x0650 )
#define _SADRSB         _SA1111( 0x0654 )
#define _SADRCB         _SA1111( 0x0658 )
#define _SAITR          _SA1111( 0x065c )
#define _SADR           _SA1111( 0x0680 )

#if LANGUAGE == C

#define SACR0		__CCREG(0x0600)
#define SACR1		__CCREG(0x0604)
#define SACR2		__CCREG(0x0608)
#define SASR0		__CCREG(0x060c)
#define SASR1		__CCREG(0x0610)
#define SASCR		__CCREG(0x0618)
#define L3_CAR		__CCREG(0x061c)
#define L3_CDR		__CCREG(0x0620)
#define ACCAR		__CCREG(0x0624)
#define ACCDR		__CCREG(0x0628)
#define ACSAR		__CCREG(0x062c)
#define ACSDR		__CCREG(0x0630)
#define SADTCS		__CCREG(0x0634)
#define SADTSA		__CCREG(0x0638)
#define SADTCA		__CCREG(0x063c)
#define SADTSB		__CCREG(0x0640)
#define SADTCB		__CCREG(0x0644)
#define SADRCS		__CCREG(0x0648)
#define SADRSA		__CCREG(0x064c)
#define SADRCA		__CCREG(0x0650)
#define SADRSB		__CCREG(0x0654)
#define SADRCB		__CCREG(0x0658)
#define SAITR		__CCREG(0x065c)
#define SADR		__CCREG(0x0680)

#endif  /* LANGUAGE == C */

#define SACR0_ENB	(1<<0)
#define SACR0_BCKD	(1<<2)
#define SACR0_RST	(1<<3)

#define SACR1_AMSL	(1<<0)
#define SACR1_L3EN	(1<<1)
#define SACR1_L3MB	(1<<2)
#define SACR1_DREC	(1<<3)
#define SACR1_DRPL	(1<<4)
#define SACR1_ENLBF	(1<<5)

#define SACR2_TS3V	(1<<0)
#define SACR2_TS4V	(1<<1)
#define SACR2_WKUP	(1<<2)
#define SACR2_DREC	(1<<3)
#define SACR2_DRPL	(1<<4)
#define SACR2_ENLBF	(1<<5)
#define SACR2_RESET	(1<<6)

#define SASR0_TNF	(1<<0)
#define SASR0_RNE	(1<<1)
#define SASR0_BSY	(1<<2)
#define SASR0_TFS	(1<<3)
#define SASR0_RFS	(1<<4)
#define SASR0_TUR	(1<<5)
#define SASR0_ROR	(1<<6)
#define SASR0_L3WD	(1<<16)
#define SASR0_L3RD	(1<<17)

#define SASR1_TNF	(1<<0)
#define SASR1_RNE	(1<<1)
#define SASR1_BSY	(1<<2)
#define SASR1_TFS	(1<<3)
#define SASR1_RFS	(1<<4)
#define SASR1_TUR	(1<<5)
#define SASR1_ROR	(1<<6)
#define SASR1_CADT	(1<<16)
#define SASR1_SADR	(1<<17)
#define SASR1_RSTO	(1<<18)
#define SASR1_CLPM	(1<<19)
#define SASR1_CRDY	(1<<20)
#define SASR1_RS3V	(1<<21)
#define SASR1_RS4V	(1<<22)

#define SASCR_TUR	(1<<5)
#define SASCR_ROR	(1<<6)
#define SASCR_DTS	(1<<16)
#define SASCR_RDD	(1<<17)
#define SASCR_STO	(1<<18)

#define SADTCS_TDEN	(1<<0)
#define SADTCS_TDIE	(1<<1)
#define SADTCS_TDBDA	(1<<3)
#define SADTCS_TDSTA	(1<<4)
#define SADTCS_TDBDB	(1<<5)
#define SADTCS_TDSTB	(1<<6)
#define SADTCS_TBIU	(1<<7)

#define SADRCS_RDEN	(1<<0)
#define SADRCS_RDIE	(1<<1)
#define SADRCS_RDBDA	(1<<3)
#define SADRCS_RDSTA	(1<<4)
#define SADRCS_RDBDB	(1<<5)
#define SADRCS_RDSTB	(1<<6)
#define SADRCS_RBIU	(1<<7)

#define SAD_CS_DEN	(1<<0)
#define SAD_CS_DIE	(1<<1)	/* Not functional on metal 1 */
#define SAD_CS_DBDA	(1<<3)	/* Not functional on metal 1 */
#define SAD_CS_DSTA	(1<<4)
#define SAD_CS_DBDB	(1<<5)	/* Not functional on metal 1 */
#define SAD_CS_DSTB	(1<<6)
#define SAD_CS_BIU	(1<<7)	/* Not functional on metal 1 */

#define SAITR_TFS	(1<<0)
#define SAITR_RFS	(1<<1)
#define SAITR_TUR	(1<<2)
#define SAITR_ROR	(1<<3)
#define SAITR_CADT	(1<<4)
#define SAITR_SADR	(1<<5)
#define SAITR_RSTO	(1<<6)
#define SAITR_TDBDA	(1<<8)
#define SAITR_TDBDB	(1<<9)
#define SAITR_RDBDA	(1<<10)
#define SAITR_RDBDB	(1<<11)

/*
 * General-Purpose I/O Interface
 *
 * Registers
 *    PA_DDR		GPIO Block A Data Direction
 *    PA_DRR/PA_DWR	GPIO Block A Data Value Register (read/write)
 *    PA_SDR		GPIO Block A Sleep Direction
 *    PA_SSR		GPIO Block A Sleep State
 *    PB_DDR		GPIO Block B Data Direction
 *    PB_DRR/PB_DWR	GPIO Block B Data Value Register (read/write)
 *    PB_SDR		GPIO Block B Sleep Direction
 *    PB_SSR		GPIO Block B Sleep State
 *    PC_DDR		GPIO Block C Data Direction
 *    PC_DRR/PC_DWR	GPIO Block C Data Value Register (read/write)
 *    PC_SDR		GPIO Block C Sleep Direction
 *    PC_SSR		GPIO Block C Sleep State
 */

#define _PA_DDR		_SA1111( 0x1000 )
#define _PA_DRR		_SA1111( 0x1004 )
#define _PA_DWR		_SA1111( 0x1004 )
#define _PA_SDR		_SA1111( 0x1008 )
#define _PA_SSR		_SA1111( 0x100c )
#define _PB_DDR		_SA1111( 0x1010 )
#define _PB_DRR		_SA1111( 0x1014 )
#define _PB_DWR		_SA1111( 0x1014 )
#define _PB_SDR		_SA1111( 0x1018 )
#define _PB_SSR		_SA1111( 0x101c )
#define _PC_DDR		_SA1111( 0x1020 )
#define _PC_DRR		_SA1111( 0x1024 )
#define _PC_DWR		_SA1111( 0x1024 )
#define _PC_SDR		_SA1111( 0x1028 )
#define _PC_SSR		_SA1111( 0x102c )

#if LANGUAGE == C

#define PA_DDR		__CCREG(0x1000)
#define PA_DRR		__CCREG(0x1004)
#define PA_DWR		__CCREG(0x1004)
#define PA_SDR		__CCREG(0x1008)
#define PA_SSR		__CCREG(0x100c)
#define PB_DDR		__CCREG(0x1010)
#define PB_DRR		__CCREG(0x1014)
#define PB_DWR		__CCREG(0x1014)
#define PB_SDR		__CCREG(0x1018)
#define PB_SSR		__CCREG(0x101c)
#define PC_DDR		__CCREG(0x1020)
#define PC_DRR		__CCREG(0x1024)
#define PC_DWR		__CCREG(0x1024)
#define PC_SDR		__CCREG(0x1028)
#define PC_SSR		__CCREG(0x102c)

#endif  /* LANGUAGE == C */

/*
 * Interrupt Controller
 *
 * Registers
 *    INTTEST0		Test register 0
 *    INTTEST1		Test register 1
 *    INTEN0		Interrupt Enable register 0
 *    INTEN1		Interrupt Enable register 1
 *    INTPOL0		Interrupt Polarity selection 0
 *    INTPOL1		Interrupt Polarity selection 1
 *    INTTSTSEL		Interrupt source selection
 *    INTSTATCLR0	Interrupt Status/Clear 0
 *    INTSTATCLR1	Interrupt Status/Clear 1
 *    INTSET0		Interrupt source set 0
 *    INTSET1		Interrupt source set 1
 *    WAKE_EN0		Wake-up source enable 0
 *    WAKE_EN1		Wake-up source enable 1
 *    WAKE_POL0		Wake-up polarity selection 0
 *    WAKE_POL1		Wake-up polarity selection 1
 */

#define SA1111_INTTEST0		0x1600
#define SA1111_INTTEST1		0x1604
#define SA1111_INTEN0		0x1608
#define SA1111_INTEN1		0x160c
#define SA1111_INTPOL0		0x1610
#define SA1111_INTPOL1		0x1614
#define SA1111_INTTSTSEL	0x1618
#define SA1111_INTSTATCLR0	0x161c
#define SA1111_INTSTATCLR1	0x1620
#define SA1111_INTSET0		0x1624
#define SA1111_INTSET1		0x1628
#define SA1111_WAKE_EN0		0x162c
#define SA1111_WAKE_EN1		0x1630
#define SA1111_WAKE_POL0	0x1634
#define SA1111_WAKE_POL1	0x1638

#define _INTTEST0	_SA1111(SA1111_INTTEST0)
#define _INTTEST1	_SA1111(SA1111_INTTEST1)
#define _INTEN0		_SA1111(SA1111_INTEN0)
#define _INTEN1		_SA1111(SA1111_INTEN1)
#define _INTPOL0	_SA1111(SA1111_INTPOL0)
#define _INTPOL1	_SA1111(SA1111_INTPOL1)
#define _INTTSTSEL	_SA1111(SA1111_INTTSTSEL)
#define _INTSTATCLR0	_SA1111(SA1111_INTSTATCLR0)
#define _INTSTATCLR1	_SA1111(SA1111_INTSTATCLR1)
#define _INTSET0	_SA1111(SA1111_INTSET0)
#define _INTSET1	_SA1111(SA1111_INTSET1)
#define _WAKE_EN0	_SA1111(SA1111_WAKE_EN0)
#define _WAKE_EN1	_SA1111(SA1111_WAKE_EN1)
#define _WAKE_POL0	_SA1111(SA1111_WAKE_POL0)
#define _WAKE_POL1	_SA1111(SA1111_WAKE_POL1)

#if LANGUAGE == C

#define INTTEST0	__CCREG(SA1111_INTTEST0)
#define INTTEST1	__CCREG(SA1111_INTTEST1)
#define INTEN0		__CCREG(SA1111_INTEN0)
#define INTEN1		__CCREG(SA1111_INTEN1)
#define INTPOL0		__CCREG(SA1111_INTPOL0)
#define INTPOL1		__CCREG(SA1111_INTPOL1)
#define INTTSTSEL	__CCREG(SA1111_INTTSTSEL)
#define INTSTATCLR0	__CCREG(SA1111_INTSTATCLR0)
#define INTSTATCLR1	__CCREG(SA1111_INTSTATCLR1)
#define INTSET0		__CCREG(SA1111_INTSET0)
#define INTSET1		__CCREG(SA1111_INTSET1)
#define WAKE_EN0	__CCREG(SA1111_WAKE_EN0)
#define WAKE_EN1	__CCREG(SA1111_WAKE_EN1)
#define WAKE_POL0	__CCREG(SA1111_WAKE_POL0)
#define WAKE_POL1	__CCREG(SA1111_WAKE_POL1)

#endif  /* LANGUAGE == C */

/*
 * PS/2 Trackpad and Mouse Interfaces
 *
 * Registers   (prefix kbd applies to trackpad interface, mse to mouse)
 *    KBDCR     Control Register
 *    KBDSTAT       Status Register
 *    KBDDATA       Transmit/Receive Data register
 *    KBDCLKDIV     Clock Division Register
 *    KBDPRECNT     Clock Precount Register
 *    KBDTEST1      Test register 1
 *    KBDTEST2      Test register 2
 *    KBDTEST3      Test register 3
 *    KBDTEST4      Test register 4
 *    MSECR
 *    MSESTAT
 *    MSEDATA
 *    MSECLKDIV
 *    MSEPRECNT
 *    MSETEST1
 *    MSETEST2
 *    MSETEST3
 *    MSETEST4
 *
 */

#define _KBD( x )   _SA1111( 0x0A00 )
#define _MSE( x )   _SA1111( 0x0C00 )

#define _KBDCR	    _SA1111( 0x0A00 )
#define _KBDSTAT    _SA1111( 0x0A04 )
#define _KBDDATA    _SA1111( 0x0A08 )
#define _KBDCLKDIV  _SA1111( 0x0A0C )
#define _KBDPRECNT  _SA1111( 0x0A10 )
#define _MSECR	    _SA1111( 0x0C00 )
#define _MSESTAT    _SA1111( 0x0C04 )
#define _MSEDATA    _SA1111( 0x0C08 )
#define _MSECLKDIV  _SA1111( 0x0C0C )
#define _MSEPRECNT  _SA1111( 0x0C10 )

#if ( LANGUAGE == C )

#define KBDCR		__CCREG(0x0a00)
#define KBDSTAT		__CCREG(0x0a04)
#define KBDDATA		__CCREG(0x0a08)
#define KBDCLKDIV	__CCREG(0x0a0c)
#define KBDPRECNT	__CCREG(0x0a10)
#define MSECR		__CCREG(0x0c00)
#define MSESTAT		__CCREG(0x0c04)
#define MSEDATA		__CCREG(0x0c08)
#define MSECLKDIV	__CCREG(0x0c0c)
#define MSEPRECNT	__CCREG(0x0c10)

#define KBDCR_ENA        0x08
#define KBDCR_FKD        0x02
#define KBDCR_FKC        0x01

#define KBDSTAT_TXE      0x80
#define KBDSTAT_TXB      0x40
#define KBDSTAT_RXF      0x20
#define KBDSTAT_RXB      0x10
#define KBDSTAT_ENA      0x08
#define KBDSTAT_RXP      0x04
#define KBDSTAT_KBD      0x02
#define KBDSTAT_KBC      0x01

#define KBDCLKDIV_DivVal     Fld(4,0)

#define MSECR_ENA        0x08
#define MSECR_FKD        0x02
#define MSECR_FKC        0x01

#define MSESTAT_TXE      0x80
#define MSESTAT_TXB      0x40
#define MSESTAT_RXF      0x20
#define MSESTAT_RXB      0x10
#define MSESTAT_ENA      0x08
#define MSESTAT_RXP      0x04
#define MSESTAT_MSD      0x02
#define MSESTAT_MSC      0x01

#define MSECLKDIV_DivVal     Fld(4,0)

#define KBDTEST1_CD      0x80
#define KBDTEST1_RC1         0x40
#define KBDTEST1_MC      0x20
#define KBDTEST1_C       Fld(2,3)
#define KBDTEST1_T2      0x40
#define KBDTEST1_T1      0x20
#define KBDTEST1_T0      0x10
#define KBDTEST2_TICBnRES    0x08
#define KBDTEST2_RKC         0x04
#define KBDTEST2_RKD         0x02
#define KBDTEST2_SEL         0x01
#define KBDTEST3_ms_16       0x80
#define KBDTEST3_us_64       0x40
#define KBDTEST3_us_16       0x20
#define KBDTEST3_DIV8        0x10
#define KBDTEST3_DIn         0x08
#define KBDTEST3_CIn         0x04
#define KBDTEST3_KD      0x02
#define KBDTEST3_KC      0x01
#define KBDTEST4_BC12        0x80
#define KBDTEST4_BC11        0x40
#define KBDTEST4_TRES        0x20
#define KBDTEST4_CLKOE       0x10
#define KBDTEST4_CRES        0x08
#define KBDTEST4_RXB         0x04
#define KBDTEST4_TXB         0x02
#define KBDTEST4_SRX         0x01

#define MSETEST1_CD      0x80
#define MSETEST1_RC1         0x40
#define MSETEST1_MC      0x20
#define MSETEST1_C       Fld(2,3)
#define MSETEST1_T2      0x40
#define MSETEST1_T1      0x20
#define MSETEST1_T0      0x10
#define MSETEST2_TICBnRES    0x08
#define MSETEST2_RKC         0x04
#define MSETEST2_RKD         0x02
#define MSETEST2_SEL         0x01
#define MSETEST3_ms_16       0x80
#define MSETEST3_us_64       0x40
#define MSETEST3_us_16       0x20
#define MSETEST3_DIV8        0x10
#define MSETEST3_DIn         0x08
#define MSETEST3_CIn         0x04
#define MSETEST3_KD      0x02
#define MSETEST3_KC      0x01
#define MSETEST4_BC12        0x80
#define MSETEST4_BC11        0x40
#define MSETEST4_TRES        0x20
#define MSETEST4_CLKOE       0x10
#define MSETEST4_CRES        0x08
#define MSETEST4_RXB         0x04
#define MSETEST4_TXB         0x02
#define MSETEST4_SRX         0x01

#endif  /* LANGUAGE == C */

/*
 * PCMCIA Interface
 *
 * Registers
 *    PCSR	Status Register
 *    PCCR	Control Register
 *    PCSSR	Sleep State Register
 */

#define _PCCR		_SA1111( 0x1800 )
#define _PCSSR		_SA1111( 0x1804 )
#define _PCSR		_SA1111( 0x1808 )

#if LANGUAGE == C

#define PCCR		__CCREG(0x1800)
#define PCSSR		__CCREG(0x1804)
#define PCSR		__CCREG(0x1808)

#endif  /* LANGUAGE == C */

#define PCSR_S0_READY	(1<<0)
#define PCSR_S1_READY	(1<<1)
#define PCSR_S0_DETECT	(1<<2)
#define PCSR_S1_DETECT	(1<<3)
#define PCSR_S0_VS1	(1<<4)
#define PCSR_S0_VS2	(1<<5)
#define PCSR_S1_VS1	(1<<6)
#define PCSR_S1_VS2	(1<<7)
#define PCSR_S0_WP	(1<<8)
#define PCSR_S1_WP	(1<<9)
#define PCSR_S0_BVD1	(1<<10)
#define PCSR_S0_BVD2	(1<<11)
#define PCSR_S1_BVD1	(1<<12)
#define PCSR_S1_BVD2	(1<<13)

#define PCCR_S0_RST	(1<<0)
#define PCCR_S1_RST	(1<<1)
#define PCCR_S0_FLT	(1<<2)
#define PCCR_S1_FLT	(1<<3)
#define PCCR_S0_PWAITEN	(1<<4)
#define PCCR_S1_PWAITEN	(1<<5)
#define PCCR_S0_PSE	(1<<6)
#define PCCR_S1_PSE	(1<<7)

#define PCSSR_S0_SLEEP	(1<<0)
#define PCSSR_S1_SLEEP	(1<<1)

int sa1111_check_dma_bug(dma_addr_t addr);

#endif  /* _ASM_ARCH_SA1111 */
