/*
 *
 * PATCHES MAGIC                LEVEL   PATCH THAT GOT US HERE
 * --------------------         -----   ----------------------
 * CURRENT PATCH LEVEL:         1       00162
 * --------------------         -----   ----------------------
 *
 * 26 May 93	Holger Veit		added more 8042 defines
 *
 * Keyboard definitions
 */

/* Reference:	IBM AT Technical Reference Manual, 
 * pp. 1-38 to 1-43, 4-3 to 4-22 
 */

/* commands sent to KBCMDP */

#define	KBC_CMDREAD	0x20	/* read kbd cntrl command byte */
#define KBC_CMDWRITE	0x60	/* == LD_CMDBYTE in kd.h, write command */
#define KBC_SELFTEST	0xAA	/* perform self test, returns 55 when ok */
#define KBC_IFTEST	0xAB	/* perform interface test */
#define	KBC_DIAGDUMP	0xAC	/* send 19 status bytes to system */
#define KBC_DISKBD	0xAD	/* disable keyboard */
#define KBC_ENAKBD	0xAE	/* enable keyboard */
#define KBC_RDINP	0xC0	/* read input port */
#define	KBC_RDID	0xC4	/* read keyboard ID */
#define KBC_RDOUTP	0xD0	/* read output port */
#define KBC_WROUTP	0xD1	/* write output port */
#define KBC_RDTINP	0xE0	/* read test inputs */

/* commands sent to KBDATAP */
#define	KBC_STSIND	0xED	/* set keyboard status indicators */
#define KBC_ECHO	0xEE	/* reply with 0xEE */
#define KBC_SETTPM	0xF3	/* Set typematic rate/delay */
#define	KBC_ENABLE	0xF4	/* Start scanning */
#define KBC_SETDEFD	0xF5	/* =KBC_SETDEF, but disable scanning */
#define KBC_SETDEF	0xF6	/* Set power on defaults */
#define KBC_RESEND	0xFE	/* system wants keyboard to resend last code */
#define	KBC_RESET	0xFF	/* Reset the keyboard */

/* responses */
#define	KBR_OVERRUN	0x00	/* Keyboard flooded */
#define KBR_STOK	0x55	/* Selftest ok response */
#define KBR_IFOK	0x00	/* Interface test ok */
#define	KBR_IFCL_SA0	0x01	/* Clock Stuck-at-0 fault */
#define	KBR_IFCL_SA1	0x02	/* Clock Stuck-at-1 fault */
#define	KBR_IFDA_SA0	0x03	/* Data Stuck-at-0 fault */
#define	KBR_IFDA_SA1	0x04	/* Data Stuck-at-1 fault */
#define	KBR_RSTDONE	0xAA	/* Keyboard reset (BAT) complete */
#define KBR_E0		0xE0	/* Extended prefix */
#define KBR_E1		0xE1	/* BREAK'S HIT :-( */
#define KBR_ECHO	0xEE	/* Echo response */
#define KBR_F0		0xF0	/* Break code prefix */
#define	KBR_ACK		0xFA	/* Keyboard did receive command */
#define KBR_BATFAIL	0xFC	/* BAT failed */
#define KBR_DIAGFAIL	0xFD	/* Diagnostic failed response */
#define	KBR_RESEND	0xFE	/* Keyboard needs resend of command */
