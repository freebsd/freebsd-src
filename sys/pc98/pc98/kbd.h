/*
 * Keyboard definitions
 *	from: unknown origin, 386BSD 0.1
 *	$Id: kbd.h,v 1.4 1995/05/30 08:02:38 rgrimes Exp $
 */

#ifndef _I386_ISA_KBD_H_
#define _I386_ISA_KBD_H_ 1

/* Reference:	IBM AT Technical Reference Manual,
 * pp. 1-38 to 1-43, 4-3 to 4-22
 */

/* commands and responses */
#define	KBC_RESET	0xFF	/* Reset the keyboard */
#define	KBC_STSIND	0xED	/* set keyboard status indicators */
#define	KBR_OVERRUN	0xFE	/* Keyboard flooded */
#define	KBR_RESEND	0xFE	/* Keyboard needs resend of command */
#define	KBR_ACK		0xFA	/* Keyboard did receive command */
#define	KBR_RSTDONE	0xAA	/* Keyboard reset complete */
#endif /* _I386_ISA_KBD_H_ */
