/*
 *	$Id: i8042.h,v 1.2 1993/10/16 13:48:47 rgrimes Exp $
 */

#define	KBSTATP		0x64	/* kbd controller status port (I) */
#define	 KBS_DIB	0x01	/* kbd data in buffer */
#define	 KBS_IBF	0x02	/* kbd input buffer low */
#define	 KBS_WARM	0x04	/* kbd input buffer low */
#define	 KBS_OCMD	0x08	/* kbd output buffer has command */
#define	 KBS_NOSEC	0x10	/* kbd security lock not engaged */
#define	 KBS_TERR	0x20	/* kbd transmission error */
#define	 KBS_RERR	0x40	/* kbd receive error */
#define	 KBS_PERR	0x80	/* kbd parity error */

#define	KBCMDP		0x64	/* kbd controller port (O) */
#define	KBDATAP		0x60	/* kbd data port (I) */
#define	KBOUTP		0x60	/* kbd data port (O) */

#define	K_LDCMDBYTE	0x60

#define	KC8_TRANS	0x40	/* convert to old scan codes */
#define	KC8_OLDPC	0x20	/* old 9bit codes instead of new 11bit */
#define	KC8_DISABLE	0x10	/* disable keyboard */
#define	KC8_IGNSEC	0x08	/* ignore security lock */
#define	KC8_CPU		0x04	/* exit from protected mode reset */
#define	KC8_IEN		0x01	/* enable interrupt */
#define	CMDBYTE	(KC8_TRANS|KC8_IGNSEC|KC8_CPU|KC8_IEN)
