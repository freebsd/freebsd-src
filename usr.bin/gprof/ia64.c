#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/usr.bin/gprof/ia64.c,v 1.2 2002/06/30 05:21:21 obrien Exp $");

#include "gprof.h"

/*
 * gprof -c isn't currently supported...
 */
findcall( parentp , p_lowpc , p_highpc )
    nltype		*parentp;
    unsigned long	p_lowpc;
    unsigned long	p_highpc;
{
}
