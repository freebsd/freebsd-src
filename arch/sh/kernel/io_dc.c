/*
 *	$Id: io_dc.c,v 1.2 2001/05/24 00:13:47 gniibe Exp $
 *	I/O routines for SEGA Dreamcast
 */

#include <asm/io.h>
#include <asm/machvec.h>

unsigned long dreamcast_isa_port2addr(unsigned long offset)
{
	return offset + 0xa0000000;
}
