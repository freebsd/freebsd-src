static char     rcsid[] = "@(#)$Id: rstcode.c,v 1.1 1995/01/25 14:06:18 jkr Exp jkr $";
/*******************************************************************************
 *  II - Version 0.1 $Revision: 1.1 $   $State: Exp $
 *
 * Copyright 1994 Dietmar Friede
 *******************************************************************************
 * Bug reports, patches, comments, suggestions should be sent to:
 *
 *	jkr@saarlink.de or jkrause@guug.de
 *
 *******************************************************************************
 * $Log: rstcode.c,v $
 *
 ******************************************************************************/
#include <sys/types.h>
#include <machine/endian.h>
#include <stdio.h>

struct head
{
	u_long          len;
	u_long          sig;
	char            nam[8];
	char            ver[5];
	u_char          typ;
}               head =
{
	0, 0, "RESETCOD", "0.000", 3
};



const char      ResetCode[] = {
	0x00, 0x00, 0x00, 0x00,	/* SP */
	0x00, 0x00, 0x00, 0x08,	/* PC */
	0x20, 0x7c, 0xff, 0xff, 0xff, 0xcc,	/* movea.l #0xffffffcc,a0 */
	0x20, 0xbc, 0xff, 0xf9, 0xe6, 0xff,	/* move.l #0xffff9e6ff,(a0) */
	0x51, 0x88,		/* subq.q #8,a0 */
	0x21, 0x3c, 0x00, 0x01, 0xe6, 0xff,	/* move.l #0x1e6ff,-(a0) */
	0x20, 0x38, 0x07, 0xfc,	/* move.l $7fc,d0 ; Reset PC DPRAM */
	0x10, 0x39, 0xFF, 0xF8, 0x07, 0xff,	/* move.b $fff807ff,d0 Reset
						 * DSP DPRAM */
	0x42, 0xb8, 0x00, 0x04,	/* clr.l $4 */
0x4e, 0x72, 0x27, 0x00};	/* stop #$2700 */

main()
{
	head.len = ntohl(0x16 + sizeof(ResetCode));
	fwrite(&head, 1, 0x16, stdout);
	fwrite(ResetCode, 1, sizeof(ResetCode), stdout);
}
