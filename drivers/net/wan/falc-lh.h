/*
 *	Defines for comx-hw-slicecom.c - FALC-LH specific
 *
 *	Author:		Bartok Istvan <bartoki@itc.hu>
 *	Last modified:	Mon Feb  7 20:00:38 CET 2000
 *
 *	:set tabstop=6
 */

/*
 *	Control register offsets on the LBI (page 90)
 *	use it like:
 *	lbi[ MODE ] = 0x34;
 */

#define MODE	0x03
#define IPC		0x08
#define IMR0	0x14	/* Interrupt Mask Register 0	*/
#define IMR1	0x15
#define IMR2	0x16
#define IMR3	0x17
#define IMR4	0x18
#define IMR5	0x19
#define FMR0	0x1a	/* Framer Mode Register 0	*/
#define FMR1	0x1b
#define FMR2	0x1c
#define XSW		0x1e
#define XSP		0x1f
#define XC0		0x20
#define XC1		0x21
#define RC0		0x22
#define RC1		0x23
#define XPM0	0x24
#define XPM1	0x25
#define XPM2	0x26
#define TSWM	0x27
#define IDLE	0x29	/* Idle Code	*/
#define LIM0	0x34
#define LIM1	0x35
#define PCD		0x36
#define PCR		0x37
#define LIM2	0x38

/*
 *	Status registers on the LBI (page 134)
 *	these are read-only, use it like:
 *	if( lbi[ FRS0 ] ) ...
 */

#define FRS0	0x4c	/* Framer Receive Status register 0	*/
#define FRS1	0x4d	/* Framer Receive Status register 1	*/
#define FECL	0x50	/* Framing Error Counter low byte	*/ /* Counts FAS word receive errors		*/
#define FECH	0x51	/*                       high byte	*/
#define CVCL	0x52	/* Code Violation Counter low byte	*/ /* Counts bipolar and HDB3 code violations	*/
#define CVCH	0x53	/*                        high byte	*/
#define CEC1L	0x54	/* CRC4 Error Counter 1 low byte	*/ /* Counts CRC4 errors in the incoming stream	*/
#define CEC1H	0x55	/*                      high byte	*/
#define EBCL	0x56	/* E Bit error Counter low byte	*/ /* E-bits: the remote end sends them, when	*/
#define EBCH	0x57	/*                     high byte	*/ /* it detected a CRC4-error			*/
#define ISR0	0x68	/* Interrupt Status Register 0	*/
#define ISR1	0x69	/* Interrupt Status Register 1	*/
#define ISR2	0x6a	/* Interrupt Status Register 2	*/
#define ISR3	0x6b	/* Interrupt Status Register 3	*/
#define ISR5	0x6c	/* Interrupt Status Register 5	*/
#define GIS	0x6e	/* Global Interrupt Status Register	*/
#define VSTR	0x6f	/* version information */

/*
 *	Bit fields
 */

#define FRS0_LOS		(1 << 7)
#define FRS0_AIS		(1 << 6)
#define FRS0_LFA		(1 << 5)
#define FRS0_RRA		(1 << 4)
#define FRS0_AUXP		(1 << 3)
#define FRS0_NMF		(1 << 2)
#define FRS0_LMFA		(1 << 1)

#define FRS1_XLS		(1 << 1)
#define FRS1_XLO		(1)

#define ISR2_FAR		(1 << 7)
#define ISR2_LFA		(1 << 6)
#define ISR2_MFAR		(1 << 5)
#define ISR2_T400MS	(1 << 4)
#define ISR2_AIS		(1 << 3)
#define ISR2_LOS		(1 << 2)
#define ISR2_RAR		(1 << 1)
#define ISR2_RA		(1)

#define ISR3_ES		(1 << 7)
#define ISR3_SEC		(1 << 6)
#define ISR3_LMFA16	(1 << 5)
#define ISR3_AIS16	(1 << 4)
#define ISR3_RA16		(1 << 3)
#define ISR3_API		(1 << 2)
#define ISR3_RSN		(1 << 1)
#define ISR3_RSP		(1)

#define ISR5_XSP		(1 << 7)
#define ISR5_XSN		(1 << 6)
