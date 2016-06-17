/* $Id$
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (c) 1992-1997,2000-2003 Silicon Graphics, Inc. All rights reserved.
 */

#include <linux/types.h>
#include <linux/slab.h>
#include <asm/sn/sgi.h>
#include <asm/sn/driver.h>
#include <asm/sn/iograph.h>
#include <asm/sn/invent.h>
#include <asm/sn/hcl.h>
#include <asm/sn/labelcl.h>
#include <asm/sn/xtalk/xtalk.h>
#include <asm/sn/xtalk/xswitch.h>
#include <asm/sn/xtalk/xwidget.h>
#include <asm/sn/xtalk/xtalk_private.h>

#define	NEW(ptr)	(ptr = kmalloc(sizeof (*(ptr)), GFP_KERNEL))
#define	DEL(ptr)	(kfree(ptr))

/*
 * This file provides generic support for Crosstalk
 * Switches, in a way that insulates crosstalk providers
 * from specifics about the switch chips being used.
 */

#include <asm/sn/xtalk/xbow.h>
#define DEV_FUNC(dev,func)      xbow_##func

#if !defined(DEV_FUNC)
/*
 * There is more than one possible provider
 * for this platform. We need to examine the
 * master vertex of the current vertex for
 * a provider function structure, and indirect
 * through the appropriately named member.
 */
#define	DEV_FUNC(dev,func)	xwidget_to_provider_fns(dev)->func

static xswitch_provider_t *
xwidget_to_provider_fns(vertex_hdl_t xconn)
{
    vertex_hdl_t            busv;
    xswitch_info_t          xswitch_info;
    xswitch_provider_t      provider_fns;

    busv = hwgraph_connectpt_get(xconn_vhdl);
    ASSERT(busv != GRAPH_VERTEX_NONE);

    xswitch_info = xswitch_info_get(busv);
    ASSERT(xswitch_info != NULL);

    provider_fns = xswitch_info->xswitch_fns;
    ASSERT(provider_fns != NULL);

    return provider_fns;
}
#endif

#define	XSWITCH_CENSUS_BIT(port)		(1<<(port))
#define	XSWITCH_CENSUS_PORT_MIN			(0x0)
#define	XSWITCH_CENSUS_PORT_MAX			(0xF)
#define	XSWITCH_CENSUS_PORTS			(0x10)
#define	XSWITCH_WIDGET_PRESENT(infop,port)	((infop)->census & XSWITCH_CENSUS_BIT(port))

static char             xswitch_info_fingerprint[] = "xswitch_info";

struct xswitch_info_s {
    char                   *fingerprint;
    unsigned                census;
    vertex_hdl_t            vhdl[XSWITCH_CENSUS_PORTS];
    vertex_hdl_t            master_vhdl[XSWITCH_CENSUS_PORTS];
    xswitch_provider_t     *xswitch_fns;
};

xswitch_info_t
xswitch_info_get(vertex_hdl_t xwidget)
{
    xswitch_info_t          xswitch_info;

    xswitch_info = (xswitch_info_t)
	hwgraph_fastinfo_get(xwidget);

    return (xswitch_info);
}

void
xswitch_info_vhdl_set(xswitch_info_t xswitch_info,
		      xwidgetnum_t port,
		      vertex_hdl_t xwidget)
{
#if XSWITCH_CENSUS_PORT_MIN
    if (port < XSWITCH_CENSUS_PORT_MIN)
	return;
#endif
    if (port > XSWITCH_CENSUS_PORT_MAX)
	return;

    xswitch_info->vhdl[port - XSWITCH_CENSUS_PORT_MIN] = xwidget;
}

vertex_hdl_t
xswitch_info_vhdl_get(xswitch_info_t xswitch_info,
		      xwidgetnum_t port)
{
#if XSWITCH_CENSUS_PORT_MIN
    if (port < XSWITCH_CENSUS_PORT_MIN)
	return GRAPH_VERTEX_NONE;
#endif
    if (port > XSWITCH_CENSUS_PORT_MAX)
	return GRAPH_VERTEX_NONE;

    return xswitch_info->vhdl[port - XSWITCH_CENSUS_PORT_MIN];
}

/*
 * Some systems may allow for multiple switch masters.  On such systems,
 * we assign a master for each port on the switch.  These interfaces
 * establish and retrieve that assignment.
 */
void
xswitch_info_master_assignment_set(xswitch_info_t xswitch_info,
				   xwidgetnum_t port,
				   vertex_hdl_t master_vhdl)
{
#if XSWITCH_CENSUS_PORT_MIN
    if (port < XSWITCH_CENSUS_PORT_MIN)
	return;
#endif
    if (port > XSWITCH_CENSUS_PORT_MAX)
	return;

    xswitch_info->master_vhdl[port - XSWITCH_CENSUS_PORT_MIN] = master_vhdl;
}

vertex_hdl_t
xswitch_info_master_assignment_get(xswitch_info_t xswitch_info,
				   xwidgetnum_t port)
{
#if XSWITCH_CENSUS_PORT_MIN
    if (port < XSWITCH_CENSUS_PORT_MIN)
	return GRAPH_VERTEX_NONE;
#endif
    if (port > XSWITCH_CENSUS_PORT_MAX)
	return GRAPH_VERTEX_NONE;

    return xswitch_info->master_vhdl[port - XSWITCH_CENSUS_PORT_MIN];
}

void
xswitch_info_set(vertex_hdl_t xwidget, xswitch_info_t xswitch_info)
{
    xswitch_info->fingerprint = xswitch_info_fingerprint;
    hwgraph_fastinfo_set(xwidget, (arbitrary_info_t) xswitch_info);
}

xswitch_info_t
xswitch_info_new(vertex_hdl_t xwidget)
{
    xswitch_info_t          xswitch_info;

    xswitch_info = xswitch_info_get(xwidget);
    if (xswitch_info == NULL) {
	int                     port;

	NEW(xswitch_info);
	xswitch_info->census = 0;
	for (port = XSWITCH_CENSUS_PORT_MIN;
	     port <= XSWITCH_CENSUS_PORT_MAX;
	     port++) {
	    xswitch_info_vhdl_set(xswitch_info, port,
				  GRAPH_VERTEX_NONE);

	    xswitch_info_master_assignment_set(xswitch_info,
					       port,
					       GRAPH_VERTEX_NONE);
	}
	xswitch_info_set(xwidget, xswitch_info);
    }
    return xswitch_info;
}

void
xswitch_provider_register(vertex_hdl_t busv,
			  xswitch_provider_t * xswitch_fns)
{
    xswitch_info_t          xswitch_info = xswitch_info_get(busv);

    ASSERT(xswitch_info);
    xswitch_info->xswitch_fns = xswitch_fns;
}

void
xswitch_info_link_is_ok(xswitch_info_t xswitch_info, xwidgetnum_t port)
{
    xswitch_info->census |= XSWITCH_CENSUS_BIT(port);
}

int
xswitch_info_link_ok(xswitch_info_t xswitch_info, xwidgetnum_t port)
{
#if XSWITCH_CENSUS_PORT_MIN
    if (port < XSWITCH_CENSUS_PORT_MIN)
	return 0;
#endif

    if (port > XSWITCH_CENSUS_PORT_MAX)
	return 0;

    return (xswitch_info->census & XSWITCH_CENSUS_BIT(port));
}

int
xswitch_reset_link(vertex_hdl_t xconn_vhdl)
{
    return DEV_FUNC(xconn_vhdl, reset_link)
	(xconn_vhdl);
}
