/*
 * Keyboard definitions
 */

#ifndef _PC98_PC98_KBD_H_
#define _PC98_PC98_KBD_H_ 1

/* commands and responses */
#define	KBC_RESET	0xFF	/* Reset the keyboard */
#define	KBC_STSIND	0xED	/* set keyboard status indicators */
#define	KBR_OVERRUN	0xFE	/* Keyboard flooded */
#define	KBR_RESEND	0xFE	/* Keyboard needs resend of command */
#define	KBR_ACK		0xFA	/* Keyboard did receive command */
#define	KBR_RSTDONE	0xAA	/* Keyboard reset complete */
#endif /* _PC98_PC98_KBD_H_ */
