#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/usr.bin/gprof/i386.c,v 1.2 2002/10/16 13:50:09 charnier Exp $");

#include "gprof.h"

/*
 * gprof -c isn't currently supported...
 */
void
findcall( parentp , p_lowpc , p_highpc )
    nltype		*parentp;
    unsigned long	p_lowpc;
    unsigned long	p_highpc;
{
}
