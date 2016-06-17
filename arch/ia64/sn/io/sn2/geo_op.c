/* $Id$
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1992 - 1997, 2000-2003 Silicon Graphics, Inc. All rights reserved.
 */

/*
 * @doc file m:hwcfg
 * DESCRIPTION:
 * 
 * This file contains routines for manipulating and generating 
 * Geographic IDs.  They are in a file by themself since they have
 * no dependencies on other modules.
 *  
 * ORIGIN:
 * 
 * New for SN2
 */

#include <linux/types.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <asm/smp.h>
#include <asm/irq.h>
#include <asm/hw_irq.h>
#include <asm/sn/types.h>
#include <asm/sn/sgi.h>
#include <asm/sn/iograph.h>
#include <asm/sn/invent.h>
#include <asm/sn/hcl.h>
#include <asm/sn/labelcl.h>
#include <asm/sn/io.h>
#include <asm/sn/sn_private.h>
#include <asm/sn/klconfig.h>
#include <asm/sn/sn_cpuid.h>
#include <asm/sn/pci/pciio.h>
#include <asm/sn/pci/pcibr.h>
#include <asm/sn/xtalk/xtalk.h>
#include <asm/sn/pci/pcibr_private.h>
#include <asm/sn/intr.h>
#include <asm/sn/sn2/shub_mmr_t.h>
#include <asm/sn/sn2/shubio.h>
#include <asm/sal.h>
#include <asm/sn/sn_sal.h>
#include <asm/sn/module.h>
#include <asm/sn/geo.h>

/********** Global functions and data (visible outside the module) ***********/

/*
 * @doc gf:geo_module
 * 
 * moduleid_t geo_module(geoid_t g)
 * 
 * DESCRIPTION:
 * 
 * Return the moduleid component of a geoid.
 *  
 * INTERNALS:
 * 
 * Return INVALID_MODULE for an invalid geoid.  Otherwise extract the
 * moduleid from the structure, and return it.
 *   
 * ORIGIN:
 * 
 * New for SN2
 */

moduleid_t
geo_module(geoid_t g)
{
    if (g.any.type == GEO_TYPE_INVALID)
	return INVALID_MODULE;
    else
	return g.any.module;
}


/*
 * @doc gf:geo_slab
 * 
 * slabid_t geo_slab(geoid_t g)
 * 
 * DESCRIPTION:
 * 
 * Return the slabid component of a geoid.
 *  
 * INTERNALS:
 * 
 * Return INVALID_SLAB for an invalid geoid.  Otherwise extract the
 * slabid from the structure, and return it.
 *   
 * ORIGIN:
 * 
 * New for SN2
 */

slabid_t
geo_slab(geoid_t g)
{
    if (g.any.type == GEO_TYPE_INVALID)
	return INVALID_SLAB;
    else
	return g.any.slab;
}


/*
 * @doc gf:geo_type
 * 
 * geo_type_t geo_type(geoid_t g)
 * 
 * DESCRIPTION:
 * 
 * Return the type component of a geoid.
 *  
 * INTERNALS:
 * 
 * Extract the type from the structure, and return it.
 *   
 * ORIGIN:
 * 
 * New for SN2
 */

geo_type_t
geo_type(geoid_t g)
{
    return g.any.type;
}


/*
 * @doc gf:geo_valid
 * 
 * int geo_valid(geoid_t g)
 * 
 * DESCRIPTION:
 * 
 * Return nonzero if g has a valid geoid type.
 *  
 * INTERNALS:
 * 
 * Test the type against GEO_TYPE_INVALID, and return the result.
 *   
 * ORIGIN:
 * 
 * New for SN2
 */

int
geo_valid(geoid_t g)
{
    return g.any.type != GEO_TYPE_INVALID;
}


/*
 * @doc gf:geo_cmp
 * 
 * int geo_cmp(geoid_t g0, geoid_t g1)
 * 
 * DESCRIPTION:
 * 
 * Compare two geoid_t values, from the coarsest field to the finest.
 * The comparison should be consistent with the physical locations of
 * of the hardware named by the geoids.
 *  
 * INTERNALS:
 * 
 * First compare the module, then the slab, type, and type-specific fields.
 *   
 * ORIGIN:
 * 
 * New for SN2
 */

int
geo_cmp(geoid_t g0, geoid_t g1)
{
    int rv;

    /* Compare the common fields */
    rv = MODULE_CMP(geo_module(g0), geo_module(g1));
    if (rv != 0)
	return rv;

    rv = geo_slab(g0) - geo_slab(g1);
    if (rv != 0)
	return rv;

    /* Within a slab, sort by type */
    rv = geo_type(g0) - geo_type(g1);
    if (rv != 0)
	return rv;

    switch(geo_type(g0)) {
    case GEO_TYPE_CPU:
	rv = g0.cpu.slice - g1.cpu.slice;
	break;

    case GEO_TYPE_IOCARD:
	rv = g0.pcicard.bus - g1.pcicard.bus;
	if (rv) break;
	rv = SLOTNUM_GETSLOT(g0.pcicard.slot) -
	    SLOTNUM_GETSLOT(g1.pcicard.slot);
	break;

    case GEO_TYPE_MEM:
	rv = g0.mem.membus - g1.mem.membus;
	if (rv) break;
	rv = g0.mem.memslot - g1.mem.memslot;
	break;

    default:
	rv = 0;
    }

    return rv;
}


/*
 * @doc gf:geo_new
 * 
 * geoid_t geo_new(geo_type_t type, ...)
 * 
 * DESCRIPTION:
 * 
 * Generate a new geoid_t value of the given type from its components.
 * Expected calling sequences:
 * \@itemize \@bullet
 * \@item
 * \@code\{geo_new(GEO_TYPE_INVALID)\}
 * \@item
 * \@code\{geo_new(GEO_TYPE_MODULE, moduleid_t m)\}
 * \@item
 * \@code\{geo_new(GEO_TYPE_NODE, moduleid_t m, slabid_t s)\}
 * \@item
 * \@code\{geo_new(GEO_TYPE_RTR, moduleid_t m, slabid_t s)\}
 * \@item
 * \@code\{geo_new(GEO_TYPE_IOCNTL, moduleid_t m, slabid_t s)\}
 * \@item
 * \@code\{geo_new(GEO_TYPE_IOCARD, moduleid_t m, slabid_t s, char bus, slotid_t slot)\}
 * \@item
 * \@code\{geo_new(GEO_TYPE_CPU, moduleid_t m, slabid_t s, char slice)\}
 * \@item
 * \@code\{geo_new(GEO_TYPE_MEM, moduleid_t m, slabid_t s, char membus, char slot)\}
 * \@end itemize
 *
 * Invalid types return a GEO_TYPE_INVALID geoid_t.
 *  
 * INTERNALS:
 * 
 * Use the type to determine which fields to expect.  Write the fields into
 * a new geoid_t and return it.  Note:  scalars smaller than an "int" are
 * promoted to "int" by the "..." operator, so we need extra casts on "char",
 * "slotid_t", and "slabid_t".
 *   
 * ORIGIN:
 * 
 * New for SN2
 */

geoid_t
geo_new(geo_type_t type, ...)
{
    va_list al;
    geoid_t g;
    memset(&g, 0, sizeof(g));

    va_start(al, type);

    /* Make sure the type is sane */
    if (type >= GEO_TYPE_MAX)
	type = GEO_TYPE_INVALID;

    g.any.type = type;
    if (type == GEO_TYPE_INVALID)
	goto done;		/* invalid geoids have no components at all */

    g.any.module = va_arg(al, moduleid_t);
    if (type == GEO_TYPE_MODULE)
	goto done;

    g.any.slab = (slabid_t)va_arg(al, int);

    /* Some types have additional components */
    switch(type) {
    case GEO_TYPE_CPU:
	g.cpu.slice = (char)va_arg(al, int);
	break;

    case GEO_TYPE_IOCARD:
	g.pcicard.bus = (char)va_arg(al, int);
	g.pcicard.slot = (slotid_t)va_arg(al, int);
	break;

    case GEO_TYPE_MEM:
	g.mem.membus = (char)va_arg(al, int);
	g.mem.memslot = (char)va_arg(al, int);
	break;

    default:
	break;
    }

 done:
    va_end(al);
    return g;
}
