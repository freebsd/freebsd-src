/* $Id$
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1992 - 1997, 2000-2003 Silicon Graphics, Inc. All rights reserved.
 */

#include <linux/types.h>
#include <linux/slab.h>
#include <linux/ctype.h>
#include <asm/sn/sgi.h>
#include <asm/sn/sn_sal.h>
#include <asm/sn/io.h>
#include <asm/sn/sn_cpuid.h>
#include <asm/sn/iograph.h>
#include <asm/sn/invent.h>
#include <asm/sn/hcl.h>
#include <asm/sn/hcl_util.h>
#include <asm/sn/labelcl.h>
#include <asm/sn/xtalk/xbow.h>
#include <asm/sn/pci/bridge.h>
#include <asm/sn/klconfig.h>
#include <asm/sn/sn_private.h>
#include <asm/sn/pci/pcibr.h>
#include <asm/sn/xtalk/xtalk.h>
#include <asm/sn/xtalk/xswitch.h>
#include <asm/sn/xtalk/xwidget.h>
#include <asm/sn/xtalk/xtalk_private.h>
#include <asm/sn/xtalk/xtalkaddrs.h>

/* #define IOGRAPH_DEBUG */
#ifdef IOGRAPH_DEBUG
#define DBG(x...) printk(x)
#else
#define DBG(x...)
#endif /* IOGRAPH_DEBUG */

/* #define PROBE_TEST */

/* At most 2 hubs can be connected to an xswitch */
#define NUM_XSWITCH_VOLUNTEER 2

/*
 * Track which hubs have volunteered to manage devices hanging off of
 * a Crosstalk Switch (e.g. xbow).  This structure is allocated,
 * initialized, and hung off the xswitch vertex early on when the
 * xswitch vertex is created.
 */
typedef struct xswitch_vol_s {
	struct semaphore xswitch_volunteer_mutex;
	int		xswitch_volunteer_count;
	vertex_hdl_t	xswitch_volunteer[NUM_XSWITCH_VOLUNTEER];
} *xswitch_vol_t;


void
xswitch_vertex_init(vertex_hdl_t xswitch)
{
	xswitch_vol_t xvolinfo;
	int rc;

	xvolinfo = snia_kmem_zalloc(sizeof(struct xswitch_vol_s));
	init_MUTEX(&xvolinfo->xswitch_volunteer_mutex);
	rc = hwgraph_info_add_LBL(xswitch, 
			INFO_LBL_XSWITCH_VOL,
			(arbitrary_info_t)xvolinfo);
	ASSERT(rc == GRAPH_SUCCESS); rc = rc;
}


/*
 * When assignment of hubs to widgets is complete, we no longer need the
 * xswitch volunteer structure hanging around.  Destroy it.
 */
static void
xswitch_volunteer_delete(vertex_hdl_t xswitch)
{
	xswitch_vol_t xvolinfo;
	int rc;
	extern void snia_kmem_free(void *ptr, size_t size);

	rc = hwgraph_info_remove_LBL(xswitch, 
				INFO_LBL_XSWITCH_VOL,
				(arbitrary_info_t *)&xvolinfo);
	snia_kmem_free(xvolinfo, sizeof(struct xswitch_vol_s));
}
/*
 * A Crosstalk master volunteers to manage xwidgets on the specified xswitch.
 */
/* ARGSUSED */
static void
volunteer_for_widgets(vertex_hdl_t xswitch, vertex_hdl_t master)
{
	xswitch_vol_t xvolinfo = NULL;
	vertex_hdl_t hubv;
	hubinfo_t hubinfo;

	(void)hwgraph_info_get_LBL(xswitch, 
				INFO_LBL_XSWITCH_VOL, 
				(arbitrary_info_t *)&xvolinfo);
	if (xvolinfo == NULL) {
	    if (!is_headless_node_vertex(master))
		    printk(KERN_WARNING
			"volunteer for widgets: vertex 0x%p has no info label",
			(void *)xswitch);
	    return;
	}

	down(&xvolinfo->xswitch_volunteer_mutex);
	ASSERT(xvolinfo->xswitch_volunteer_count < NUM_XSWITCH_VOLUNTEER);
	xvolinfo->xswitch_volunteer[xvolinfo->xswitch_volunteer_count] = master;
	xvolinfo->xswitch_volunteer_count++;

	/*
	 * if dual ported, make the lowest widgetid always be 
	 * xswitch_volunteer[0].
	 */
	if (xvolinfo->xswitch_volunteer_count == NUM_XSWITCH_VOLUNTEER) {
		hubv = xvolinfo->xswitch_volunteer[0];
		hubinfo_get(hubv, &hubinfo);
		if (hubinfo->h_widgetid != XBOW_HUBLINK_LOW) {
			xvolinfo->xswitch_volunteer[0] = 
						xvolinfo->xswitch_volunteer[1];
			xvolinfo->xswitch_volunteer[1] = hubv;
		}
	}
	up(&xvolinfo->xswitch_volunteer_mutex);
}

extern int xbow_port_io_enabled(nasid_t nasid, int widgetnum);

/*
 * Assign all the xwidgets hanging off the specified xswitch to the
 * Crosstalk masters that have volunteered for xswitch duty.
 */
/* ARGSUSED */
static void
assign_widgets_to_volunteers(vertex_hdl_t xswitch, vertex_hdl_t hubv)
{
	xswitch_info_t xswitch_info;
	xswitch_vol_t xvolinfo = NULL;
	xwidgetnum_t widgetnum;
	int num_volunteer;
	nasid_t nasid;
	hubinfo_t hubinfo;
	extern int iobrick_type_get_nasid(nasid_t);


	hubinfo_get(hubv, &hubinfo);
	nasid = hubinfo->h_nasid;
	
	xswitch_info = xswitch_info_get(xswitch);
	ASSERT(xswitch_info != NULL);

	(void)hwgraph_info_get_LBL(xswitch, 
				INFO_LBL_XSWITCH_VOL, 
				(arbitrary_info_t *)&xvolinfo);
	if (xvolinfo == NULL) {
	    if (!is_headless_node_vertex(hubv))
		    printk(KERN_WARNING
			"assign_widgets_to_volunteers:vertex 0x%p has "
			" no info label",
			(void *)xswitch);
	    return;
	}

	num_volunteer = xvolinfo->xswitch_volunteer_count;
	ASSERT(num_volunteer > 0);

	/* Assign master hub for xswitch itself.  */
	if (HUB_WIDGET_ID_MIN > 0) {
		hubv = xvolinfo->xswitch_volunteer[0];
		xswitch_info_master_assignment_set(xswitch_info, (xwidgetnum_t)0, hubv);
	}

	/*
	 * TBD: Use administrative information to alter assignment of
	 * widgets to hubs.
	 */
	for (widgetnum=HUB_WIDGET_ID_MIN; widgetnum <= HUB_WIDGET_ID_MAX; widgetnum++) {
		int i;

		/*
		 * Ignore disabled/empty ports.
		 */
		if (!xbow_port_io_enabled(nasid, widgetnum)) 
		    continue;

		/*
		 * If this is the master IO board, assign it to the same 
		 * hub that owned it in the prom.
		 */
		if (is_master_baseio_nasid_widget(nasid, widgetnum)) {
			extern nasid_t snia_get_master_baseio_nasid(void);
			for (i=0; i<num_volunteer; i++) {
				hubv = xvolinfo->xswitch_volunteer[i];
				hubinfo_get(hubv, &hubinfo);
				nasid = hubinfo->h_nasid;
				if (nasid == snia_get_master_baseio_nasid())
					goto do_assignment;
			}
			panic("Nasid == %d, console nasid == %d",
				nasid, snia_get_master_baseio_nasid());
		}

		/*
		 * Assuming that we're dual-hosted and that PCI cards 
		 * are naturally placed left-to-right, alternate PCI 
		 * buses across both Cbricks.   For Pbricks, and Ibricks,
                 * io_brick_map_widget() returns the PCI bus number
                 * associated with the given brick type and widget number.
                 * For Xbricks, it returns the XIO slot number.
		 */

		i = 0;
		if (num_volunteer > 1) {
                        int	       bt;

                       	bt = iobrick_type_get_nasid(nasid);
                        if (bt >= 0) {
			        i = io_brick_map_widget(bt, widgetnum) & 1;
                        }
                }

		hubv = xvolinfo->xswitch_volunteer[i];

do_assignment:
		/*
		 * At this point, we want to make hubv the master of widgetnum.
		 */
		xswitch_info_master_assignment_set(xswitch_info, widgetnum, hubv);
	}

	xswitch_volunteer_delete(xswitch);
}

/*
 * Let boot processor know that we're done initializing our node's IO
 * and then exit.
 */
/* ARGSUSED */
static void
io_init_done(cnodeid_t cnodeid,cpuid_t c)
{
	/* Let boot processor know that we're done. */
}

/* 
 * Probe to see if this hub's xtalk link is active.  If so,
 * return the Crosstalk Identification of the widget that we talk to.  
 * This is called before any of the Crosstalk infrastructure for 
 * this hub is set up.  It's usually called on the node that we're
 * probing, but not always.
 *
 * TBD: Prom code should actually do this work, and pass through 
 * hwid for our use.
 */
static void
early_probe_for_widget(vertex_hdl_t hubv, xwidget_hwid_t hwid)
{
	hubreg_t llp_csr_reg;
	nasid_t nasid;
	hubinfo_t hubinfo;

	hubinfo_get(hubv, &hubinfo);
	nasid = hubinfo->h_nasid;

	llp_csr_reg = REMOTE_HUB_L(nasid, IIO_LLP_CSR);
	/* 
	 * If link is up, read the widget's part number.
	 * A direct connect widget must respond to widgetnum=0.
	 */
	if (llp_csr_reg & IIO_LLP_CSR_IS_UP) {
		/* TBD: Put hub into "indirect" mode */
		/*
		 * We're able to read from a widget because our hub's 
		 * WIDGET_ID was set up earlier.
		 */
		widgetreg_t widget_id = *(volatile widgetreg_t *)
			(RAW_NODE_SWIN_BASE(nasid, 0x0) + WIDGET_ID);

		DBG("early_probe_for_widget: Hub Vertex 0x%p is UP widget_id = 0x%x Register 0x%p\n", hubv, widget_id,
		(volatile widgetreg_t *)(RAW_NODE_SWIN_BASE(nasid, 0x0) + WIDGET_ID) );

		hwid->part_num = XWIDGET_PART_NUM(widget_id);
		hwid->rev_num = XWIDGET_REV_NUM(widget_id);
		hwid->mfg_num = XWIDGET_MFG_NUM(widget_id);

		/* TBD: link reset */
	} else {

		hwid->part_num = XWIDGET_PART_NUM_NONE;
		hwid->rev_num = XWIDGET_REV_NUM_NONE;
		hwid->mfg_num = XWIDGET_MFG_NUM_NONE;
	}
}

/* Add inventory information to the widget vertex 
 * Right now (module,slot,revision) is being
 * added as inventory information.
 */
static void
xwidget_inventory_add(vertex_hdl_t 		widgetv,
		      lboard_t 			*board,
		      struct xwidget_hwid_s 	hwid)
{
	if (!board)
		return;
	/* Donot add inventory information for the baseio
	 * on a speedo with an xbox. It has already been
	 * taken care of in SN00_vmc.
	 * Speedo with xbox's baseio comes in at slot io1 (widget 9)
	 */
	device_inventory_add(widgetv,INV_IOBD,board->brd_type,
			     geo_module(board->brd_geoid),
			     SLOTNUM_GETSLOT(board->brd_slot),
			     hwid.rev_num);
}

/*
 * io_xswitch_widget_init
 *	
 */

void
io_xswitch_widget_init(vertex_hdl_t  	xswitchv,
		       vertex_hdl_t	hubv,
		       xwidgetnum_t	widgetnum)
{
	xswitch_info_t		xswitch_info;
	xwidgetnum_t		hub_widgetid;
	vertex_hdl_t		widgetv;
	cnodeid_t		cnode;
	widgetreg_t		widget_id;
	nasid_t			nasid, peer_nasid;
	struct xwidget_hwid_s 	hwid;
	hubinfo_t		hubinfo;
	/*REFERENCED*/
	int			rc;
	char 			pathname[128];
	lboard_t		*board = NULL;
	char			buffer[16];
	char			bt;
	moduleid_t		io_module;
	slotid_t get_widget_slotnum(int xbow, int widget);
	
	DBG("\nio_xswitch_widget_init: hubv 0x%p, xswitchv 0x%p, widgetnum 0x%x\n", hubv, xswitchv, widgetnum);

	/*
	 * Verify that xswitchv is indeed an attached xswitch.
	 */
	xswitch_info = xswitch_info_get(xswitchv);
	ASSERT(xswitch_info != NULL);

	hubinfo_get(hubv, &hubinfo);
	nasid = hubinfo->h_nasid;
	cnode = NASID_TO_COMPACT_NODEID(nasid);
	hub_widgetid = hubinfo->h_widgetid;

	/*
	 * Check that the widget is an io widget and is enabled
	 * on this nasid or the `peer' nasid.  The peer nasid
	 * is the other hub/bedrock connected to the xbow.
	 */
	peer_nasid = NODEPDA(cnode)->xbow_peer;
	if (peer_nasid == INVALID_NASID)
		/* If I don't have a peer, use myself. */
		peer_nasid = nasid;
	if (!xbow_port_io_enabled(nasid, widgetnum) &&
	    !xbow_port_io_enabled(peer_nasid, widgetnum)) {
		return;
	}

	if (xswitch_info_link_ok(xswitch_info, widgetnum)) {
		char			name[4];
		lboard_t dummy;


		/*
		 * If the current hub is not supposed to be the master 
		 * for this widgetnum, then skip this widget.
		 */

		if (xswitch_info_master_assignment_get(xswitch_info, widgetnum) != hubv) {
			return;
		}

		board = find_lboard_class(
				(lboard_t *)KL_CONFIG_INFO(nasid),
				KLCLASS_IOBRICK);
		if (!board && NODEPDA(cnode)->xbow_peer != INVALID_NASID) {
		    	board = find_lboard_class(
					(lboard_t *)KL_CONFIG_INFO( NODEPDA(cnode)->xbow_peer),
						KLCLASS_IOBRICK);
		}

		if (board) {
			DBG("io_xswitch_widget_init: Found KLTYPE_IOBRICK Board 0x%p brd_type 0x%x\n", board, board->brd_type);
		} else {
			DBG("io_xswitch_widget_init: FIXME did not find IOBOARD\n");
			board = &dummy;
		}


		/* Copy over the nodes' geoid info */
		{
			lboard_t *brd;

			brd = find_lboard((lboard_t *)KL_CONFIG_INFO(nasid), KLTYPE_SNIA);
			if ( brd != (lboard_t *)0 ) {
				board->brd_geoid = brd->brd_geoid;
			}
		}

		/*
 		 * Make sure we really want to say xbrick, pbrick,
		 * etc. rather than XIO, graphics, etc.
 		 */

		memset(buffer, 0, 16);
		format_module_id(buffer, geo_module(board->brd_geoid), MODULE_FORMAT_BRIEF);

		sprintf(pathname, EDGE_LBL_MODULE "/%s/" EDGE_LBL_SLAB "/%d" "/%s" "/%s/%d",
			buffer,
			geo_slab(board->brd_geoid),
			(board->brd_type == KLTYPE_IBRICK) ? EDGE_LBL_IBRICK :
			(board->brd_type == KLTYPE_PBRICK) ? EDGE_LBL_PBRICK :
			(board->brd_type == KLTYPE_PXBRICK) ? EDGE_LBL_PXBRICK :
			(board->brd_type == KLTYPE_IXBRICK) ? EDGE_LBL_IXBRICK :
			(board->brd_type == KLTYPE_CGBRICK) ? EDGE_LBL_CGBRICK :
			(board->brd_type == KLTYPE_OPUSBRICK) ? EDGE_LBL_OPUSBRICK :
			(board->brd_type == KLTYPE_XBRICK) ? EDGE_LBL_XBRICK : "?brick",
			EDGE_LBL_XTALK, widgetnum);
		
		DBG("io_xswitch_widget_init: path= %s\n", pathname);
		rc = hwgraph_path_add(hwgraph_root, pathname, &widgetv);
		
		ASSERT(rc == GRAPH_SUCCESS);

		/* This is needed to let the user programs to map the
		 * module,slot numbers to the corresponding widget numbers
		 * on the crossbow.
		 */
		device_master_set(hwgraph_connectpt_get(widgetv), hubv);
		sprintf(name, "%d", widgetnum);
		DBG("io_xswitch_widget_init: FIXME hwgraph_edge_add %s xswitchv 0x%p, widgetv 0x%p\n", name, xswitchv, widgetv);
		rc = hwgraph_edge_add(xswitchv, widgetv, name);
		
		/*
		 * crosstalk switch code tracks which
		 * widget is attached to each link.
		 */
		xswitch_info_vhdl_set(xswitch_info, widgetnum, widgetv);
		
		/*
		 * Peek at the widget to get its crosstalk part and
		 * mfgr numbers, then present it to the generic xtalk
		 * bus provider to have its driver attach routine
		 * called (or not).
		 */
		widget_id = XWIDGET_ID_READ(nasid, widgetnum);
		hwid.part_num = XWIDGET_PART_NUM(widget_id);
		hwid.rev_num = XWIDGET_REV_NUM(widget_id);
		hwid.mfg_num = XWIDGET_MFG_NUM(widget_id);
		/* Store some inventory information about
		 * the xwidget in the hardware graph.
		 */
		xwidget_inventory_add(widgetv,board,hwid);

		(void)xwidget_register(&hwid, widgetv, widgetnum,
				       hubv, hub_widgetid);

		ia64_sn_sysctl_iobrick_module_get(nasid, &io_module);
		if (io_module >= 0) {
			char			buffer[16];
			vertex_hdl_t		to, from;
			char           		*brick_name;
			extern char *iobrick_L1bricktype_to_name(int type);


			memset(buffer, 0, 16);
			format_module_id(buffer, geo_module(board->brd_geoid), MODULE_FORMAT_BRIEF);

			if ( isupper(MODULE_GET_BTCHAR(io_module)) ) {
				bt = tolower(MODULE_GET_BTCHAR(io_module));
			}
			else {
				bt = MODULE_GET_BTCHAR(io_module);
			}

			brick_name = iobrick_L1bricktype_to_name(bt);

			/* Add a helper vertex so xbow monitoring
			* can identify the brick type. It's simply
			* an edge from the widget 0 vertex to the
			*  brick vertex.
			*/

			sprintf(pathname, EDGE_LBL_HW "/" EDGE_LBL_MODULE "/%s/"
				EDGE_LBL_SLAB "/%d/"
				EDGE_LBL_NODE "/" EDGE_LBL_XTALK "/"
				"0",
				buffer, geo_slab(board->brd_geoid));
			DBG("io_xswitch_widget_init: FROM path '%s'\n", pathname);

			from = hwgraph_path_to_vertex(pathname);
			ASSERT_ALWAYS(from);

			sprintf(pathname, EDGE_LBL_HW "/" EDGE_LBL_MODULE "/%s/"
				EDGE_LBL_SLAB "/%d/"
				"%s",
				buffer, geo_slab(board->brd_geoid), brick_name);
			DBG("io_xswitch_widget_init: TO path '%s'\n", pathname);
			to = hwgraph_path_to_vertex(pathname);
			ASSERT_ALWAYS(to);
			rc = hwgraph_edge_add(from, to,
				EDGE_LBL_INTERCONNECT);
			if (rc == -EEXIST)
				goto link_done;
			if (rc != GRAPH_SUCCESS) {
				printk("%s: Unable to establish link"
					" for xbmon.", pathname);
			}
link_done:
		}

#ifdef	SN0_USE_BTE
		bte_bpush_war(cnode, (void *)board);
#endif
	}
}


static void
io_init_xswitch_widgets(vertex_hdl_t xswitchv, cnodeid_t cnode)
{
	xwidgetnum_t		widgetnum;
	
	DBG("io_init_xswitch_widgets: xswitchv 0x%p for cnode %d\n", xswitchv, cnode);

	for (widgetnum = HUB_WIDGET_ID_MIN; widgetnum <= HUB_WIDGET_ID_MAX; widgetnum++) {
		io_xswitch_widget_init(xswitchv, cnodeid_to_vertex(cnode), widgetnum);
	}
}

/*
 * For each PCI bridge connected to the xswitch, add a link from the
 * board's klconfig info to the bridge's hwgraph vertex.  This lets
 * the FRU analyzer find the bridge without traversing the hardware
 * graph and risking hangs.
 */
static void
io_link_xswitch_widgets(vertex_hdl_t xswitchv, cnodeid_t cnodeid)
{
	xwidgetnum_t		widgetnum;
	char 			pathname[128];
	vertex_hdl_t		vhdl;
	nasid_t			nasid, peer_nasid;
	lboard_t		*board;



	/* And its connected hub's nasids */
	nasid = COMPACT_TO_NASID_NODEID(cnodeid);
	peer_nasid = NODEPDA(cnodeid)->xbow_peer;

	/* 
	 * Look for paths matching "<widgetnum>/pci" under xswitchv.
	 * For every widget, init. its lboard's hwgraph link.  If the
	 * board has a PCI bridge, point the link to it.
	 */
	for (widgetnum = HUB_WIDGET_ID_MIN; widgetnum <= HUB_WIDGET_ID_MAX;
		 widgetnum++) {
		sprintf(pathname, "%d", widgetnum);
		if (hwgraph_traverse(xswitchv, pathname, &vhdl) !=
		    GRAPH_SUCCESS)
			continue;

		board = find_lboard_module((lboard_t *)KL_CONFIG_INFO(nasid),
				NODEPDA(cnodeid)->geoid);
		if (board == NULL && peer_nasid != INVALID_NASID) {
			/*
			 * Try to find the board on our peer
			 */
			board = find_lboard_module(
				(lboard_t *)KL_CONFIG_INFO(peer_nasid),
				NODEPDA(cnodeid)->geoid);
		}
		if (board == NULL) {
			printk(KERN_WARNING  "Could not find PROM info for vertex 0x%p, "
				"FRU analyzer may fail",
				(void *)vhdl);
			return;
		}

		/* Check both buses */
		sprintf(pathname, "%d/"EDGE_LBL_PCIX_0, widgetnum);
		if (hwgraph_traverse(xswitchv, pathname, &vhdl) == GRAPH_SUCCESS)
			board->brd_graph_link = vhdl;
		else {
			sprintf(pathname, "%d/"EDGE_LBL_PCIX_1, widgetnum);
			if (hwgraph_traverse(xswitchv, pathname, &vhdl) == GRAPH_SUCCESS)
				board->brd_graph_link = vhdl;
			else
				board->brd_graph_link = GRAPH_VERTEX_NONE;
		}
	}
}

/*
 * Initialize all I/O on the specified node.
 */
static void
io_init_node(cnodeid_t cnodeid)
{
	/*REFERENCED*/
	vertex_hdl_t hubv, switchv, widgetv;
	struct xwidget_hwid_s hwid;
	hubinfo_t hubinfo;
	int is_xswitch;
	nodepda_t	*npdap;
	struct semaphore *peer_sema = 0;
	uint32_t	widget_partnum;
	cpuid_t	c = 0;

	npdap = NODEPDA(cnodeid);

	/*
	 * Get the "top" vertex for this node's hardware
	 * graph; it will carry the per-hub hub-specific
	 * data, and act as the crosstalk provider master.
	 * It's canonical path is probably something of the
	 * form /hw/module/%M/slot/%d/node
	 */
	hubv = cnodeid_to_vertex(cnodeid);
	DBG("io_init_node: Initialize IO for cnode %d hubv(node) 0x%p npdap 0x%p\n", cnodeid, hubv, npdap);

	ASSERT(hubv != GRAPH_VERTEX_NONE);

	/*
	 * Read mfg info on this hub
	 */

	/* 
	 * If nothing connected to this hub's xtalk port, we're done.
	 */
	early_probe_for_widget(hubv, &hwid);
	if (hwid.part_num == XWIDGET_PART_NUM_NONE) {
#ifdef PROBE_TEST
		if ((cnodeid == 1) || (cnodeid == 2)) {
			int index;

			for (index = 0; index < 600; index++)
				DBG("Interfering with device probing!!!\n");
		}
#endif
		/* io_init_done takes cpu cookie as 2nd argument 
		 * to do a restorenoderun for the setnoderun done 
		 * at the start of this thread 
		 */
		
		DBG("**** io_init_node: Node's 0x%p hub widget has XWIDGET_PART_NUM_NONE ****\n", hubv);
		return;
		/* NOTREACHED */
	}

	/* 
	 * attach our hub_provider information to hubv,
	 * so we can use it as a crosstalk provider "master"
	 * vertex.
	 */
	xtalk_provider_register(hubv, &hub_provider);
	xtalk_provider_startup(hubv);

	/*
	 * Create a vertex to represent the crosstalk bus
	 * attached to this hub, and a vertex to be used
	 * as the connect point for whatever is out there
	 * on the other side of our crosstalk connection.
	 *
	 * Crosstalk Switch drivers "climb up" from their
	 * connection point to try and take over the switch
	 * point.
	 *
	 * Of course, the edges and verticies may already
	 * exist, in which case our net effect is just to
	 * associate the "xtalk_" driver with the connection
	 * point for the device.
	 */

	(void)hwgraph_path_add(hubv, EDGE_LBL_XTALK, &switchv);

	DBG("io_init_node: Created 'xtalk' entry to '../node/' xtalk vertex 0x%p\n", switchv);

	ASSERT(switchv != GRAPH_VERTEX_NONE);

	(void)hwgraph_edge_add(hubv, switchv, EDGE_LBL_IO);

	DBG("io_init_node: Created symlink 'io' from ../node/io to ../node/xtalk \n");

	/*
	 * We need to find the widget id and update the basew_id field
	 * accordingly. In particular, SN00 has direct connected bridge,
	 * and hence widget id is Not 0.
	 */

	widget_partnum = (((*(volatile int32_t *)(NODE_SWIN_BASE(COMPACT_TO_NASID_NODEID(cnodeid), 0) + WIDGET_ID))) & WIDGET_PART_NUM) >> WIDGET_PART_NUM_SHFT;

	if (widget_partnum == BRIDGE_WIDGET_PART_NUM ||
				widget_partnum == XBRIDGE_WIDGET_PART_NUM){
		npdap->basew_id = (((*(volatile int32_t *)(NODE_SWIN_BASE(COMPACT_TO_NASID_NODEID(cnodeid), 0) + BRIDGE_WID_CONTROL))) & WIDGET_WIDGET_ID);

		DBG("io_init_node: Found XBRIDGE widget_partnum= 0x%x\n", widget_partnum);

	} else if ((widget_partnum == XBOW_WIDGET_PART_NUM) ||
			(widget_partnum == XXBOW_WIDGET_PART_NUM) ||
			(widget_partnum == PXBOW_WIDGET_PART_NUM) ) {
		/* 
		 * Xbow control register does not have the widget ID field.
		 * So, hard code the widget ID to be zero.
		 */
		DBG("io_init_node: Found XBOW widget_partnum= 0x%x\n", widget_partnum);
		npdap->basew_id = 0;

	} else {
		npdap->basew_id = (((*(volatile int32_t *)(NODE_SWIN_BASE(COMPACT_TO_NASID_NODEID(cnodeid), 0) + BRIDGE_WID_CONTROL))) & WIDGET_WIDGET_ID);

		panic(" ****io_init_node: Unknown Widget Part Number 0x%x Widget ID 0x%x attached to Hubv 0x%p ****\n", widget_partnum, npdap->basew_id, (void *)hubv);

		/*NOTREACHED*/
	}
	{
		char widname[10];
		sprintf(widname, "%x", npdap->basew_id);
		(void)hwgraph_path_add(switchv, widname, &widgetv);
		DBG("io_init_node: Created '%s' to '..node/xtalk/' vertex 0x%p\n", widname, widgetv);
		ASSERT(widgetv != GRAPH_VERTEX_NONE);
	}
	
	nodepda->basew_xc = widgetv;

	is_xswitch = xwidget_hwid_is_xswitch(&hwid);

	/* 
	 * Try to become the master of the widget.  If this is an xswitch
	 * with multiple hubs connected, only one will succeed.  Mastership
	 * of an xswitch is used only when touching registers on that xswitch.
	 * The slave xwidgets connected to the xswitch can be owned by various
	 * masters.
	 */
	if (device_master_set(widgetv, hubv) == 0) {

		/* Only one hub (thread) per Crosstalk device or switch makes
		 * it to here.
		 */

		/* 
		 * Initialize whatever xwidget is hanging off our hub.
		 * Whatever it is, it's accessible through widgetnum 0.
		 */
		hubinfo_get(hubv, &hubinfo);

		(void)xwidget_register(&hwid, widgetv, npdap->basew_id, hubv, hubinfo->h_widgetid);

		if (!is_xswitch) {
			/* io_init_done takes cpu cookie as 2nd argument 
			 * to do a restorenoderun for the setnoderun done 
			 * at the start of this thread 
			 */
			io_init_done(cnodeid,c);
			/* NOTREACHED */
		}

		/* 
		 * Special handling for Crosstalk Switches (e.g. xbow).
		 * We need to do things in roughly the following order:
		 *	1) Initialize xswitch hardware (done above)
		 *	2) Determine which hubs are available to be widget masters
		 *	3) Discover which links are active from the xswitch
		 *	4) Assign xwidgets hanging off the xswitch to hubs
		 *	5) Initialize all xwidgets on the xswitch
		 */

		DBG("call volunteer_for_widgets\n");

		volunteer_for_widgets(switchv, hubv);

		/* If there's someone else on this crossbow, recognize him */
		if (npdap->xbow_peer != INVALID_NASID) {
			nodepda_t *peer_npdap = NODEPDA(NASID_TO_COMPACT_NODEID(npdap->xbow_peer));
			peer_sema = &peer_npdap->xbow_sema;
			DBG("call volunteer_for_widgets again\n");
			volunteer_for_widgets(switchv, peer_npdap->node_vertex);
		}

		assign_widgets_to_volunteers(switchv, hubv);

		/* Signal that we're done */
		if (peer_sema) {
			up(peer_sema);
		}
		
	}
	else {
	    /* Wait 'til master is done assigning widgets. */
	    down(&npdap->xbow_sema);
	}

#ifdef PROBE_TEST
	if ((cnodeid == 1) || (cnodeid == 2)) {
		int index;

		for (index = 0; index < 500; index++)
			DBG("Interfering with device probing!!!\n");
	}
#endif
	/* Now both nodes can safely inititialize widgets */
	io_init_xswitch_widgets(switchv, cnodeid);
	io_link_xswitch_widgets(switchv, cnodeid);

	/* io_init_done takes cpu cookie as 2nd argument 
	 * to do a restorenoderun for the setnoderun done 
	 * at the start of this thread 
	 */
	io_init_done(cnodeid,c);

	DBG("\nio_init_node: DONE INITIALIZED ALL I/O FOR CNODEID %d\n\n", cnodeid);
}


#define IOINIT_STKSZ	(16 * 1024)

#define __DEVSTR1 	"/../.master/"
#define __DEVSTR2 	"/target/"
#define __DEVSTR3 	"/lun/0/disk/partition/"
#define	__DEVSTR4	"/../ef"

/*
 * ioconfig starts numbering SCSI's at NUM_BASE_IO_SCSI_CTLR.
 */
#define NUM_BASE_IO_SCSI_CTLR 6
/*
 * This tells ioconfig where it can start numbering scsi controllers.
 * Below this base number, platform-specific handles the numbering.
 * XXX Irix legacy..controller numbering should be part of devfsd's job
 */
int num_base_io_scsi_ctlr = 2; /* used by syssgi */
vertex_hdl_t		base_io_scsi_ctlr_vhdl[NUM_BASE_IO_SCSI_CTLR];

#include <asm/sn/ioerror_handling.h>
/* #endif */

/*
 * Initialize all I/O devices.  Starting closest to nodes, probe and
 * initialize outward.
 */
void
init_all_devices(void)
{
	/* Governor on init threads..bump up when safe 
	 * (beware many devfs races) 
	 */
	cnodeid_t cnodeid, active;

	active = 0;
	for (cnodeid = 0; cnodeid < numnodes; cnodeid++) {
                DBG("init_all_devices: Calling io_init_node() for cnode %d\n", cnodeid);
                io_init_node(cnodeid);

		DBG("init_all_devices: Done io_init_node() for cnode %d\n", cnodeid);
	}

	for (cnodeid = 0; cnodeid < numnodes; cnodeid++)
		/*
	 	 * Update information generated by IO init.
		 */
		update_node_information(cnodeid);

#if HWG_PRINT
	hwgraph_print();
#endif

}

#define toint(x) ((int)(x) - (int)('0'))

void
devnamefromarcs(char *devnm)
{
	int 			val;
	char 			tmpnm[MAXDEVNAME];
	char 			*tmp1, *tmp2;
	
	val = strncmp(devnm, "dks", 3);
	if (val != 0) 
		return;
	tmp1 = devnm + 3;
	if (!isdigit(*tmp1))
		return;

	val = 0;
	while (isdigit(*tmp1)) {
		val = 10*val+toint(*tmp1);
		tmp1++;
	}

	if(*tmp1 != 'd')
		return;
	else
		tmp1++;

	if ((val < 0) || (val >= num_base_io_scsi_ctlr)) {
		int i;
		int viable_found = 0;

		DBG("Only controller numbers 0..%d  are supported for\n", NUM_BASE_IO_SCSI_CTLR-1);
		DBG("prom \"root\" variables of the form dksXdXsX.\n");
		DBG("To use another disk you must use the full hardware graph path\n\n");
		DBG("Possible controller numbers for use in 'dksXdXsX' on this system: ");
		for (i=0; i<NUM_BASE_IO_SCSI_CTLR; i++) {
			if (base_io_scsi_ctlr_vhdl[i] != GRAPH_VERTEX_NONE) {
				DBG("%d ", i);
				viable_found=1;
			}
		}
		if (viable_found)
			DBG("\n");
		else
			DBG("none found!\n");

		udelay(15000000);
		//prom_reboot();
		panic("FIXME: devnamefromarcs: should call prom_reboot here.\n");
		/* NOTREACHED */
	}
		
	ASSERT(base_io_scsi_ctlr_vhdl[val] != GRAPH_VERTEX_NONE);
	vertex_to_name(base_io_scsi_ctlr_vhdl[val],
		       tmpnm,
		       MAXDEVNAME);
	tmp2 = 	tmpnm + strlen(tmpnm);
	strcpy(tmp2, __DEVSTR2);
	tmp2 += strlen(__DEVSTR2);
	while (*tmp1 != 's') {
		if((*tmp2++ = *tmp1++) == '\0')
			return;
	}	
	tmp1++;
	strcpy(tmp2, __DEVSTR3);
	tmp2 += strlen(__DEVSTR3);
	while ( (*tmp2++ = *tmp1++) )
		;
	tmp2--;
	*tmp2++ = '/';
	strcpy(tmp2, EDGE_LBL_BLOCK);
	strcpy(devnm,tmpnm);
}

static
struct io_brick_map_s io_brick_tab[] = {

/* Ibrick widget number to PCI bus number map */
 {      MODULE_IBRICK,                          /* Ibrick type    */ 
    /*  PCI Bus #                                  Widget #       */
    {   0, 0, 0, 0, 0, 0, 0, 0,                 /* 0x0 - 0x7      */
        0,                                      /* 0x8            */
        0,                                      /* 0x9            */
        0, 0,                                   /* 0xa - 0xb      */
        0,                                      /* 0xc            */
        0,                                      /* 0xd            */
        2,                                      /* 0xe            */
        1                                       /* 0xf            */
     }
 },

/* Pbrick widget number to PCI bus number map */
 {      MODULE_PBRICK,                          /* Pbrick type    */ 
    /*  PCI Bus #                                  Widget #       */
    {   0, 0, 0, 0, 0, 0, 0, 0,                 /* 0x0 - 0x7      */
        2,                                      /* 0x8            */
        1,                                      /* 0x9            */
        0, 0,                                   /* 0xa - 0xb      */
        5,                                      /* 0xc            */
        6,                                      /* 0xd            */
        4,                                      /* 0xe            */
        3                                       /* 0xf            */
    }
 },

/* PXbrick widget number to PCI bus number map */
 {      MODULE_PXBRICK,                         /* PXbrick type   */ 
    /*  PCI Bus #                                  Widget #       */
    {   0, 0, 0, 0, 0, 0, 0, 0,                 /* 0x0 - 0x7      */
        0,                                      /* 0x8            */
        0,                                      /* 0x9            */
        0, 0,                                   /* 0xa - 0xb      */
        1,                                      /* 0xc            */
        5,                                      /* 0xd            */
        0,                                      /* 0xe            */
        3                                       /* 0xf            */
    }
 },

/* OPUSbrick widget number to PCI bus number map */
 {      MODULE_OPUSBRICK,                       /* OPUSbrick type */ 
    /*  PCI Bus #                                  Widget #       */
    {   0, 0, 0, 0, 0, 0, 0, 0,                 /* 0x0 - 0x7      */
        0,                                      /* 0x8            */
        0,                                      /* 0x9            */
        0, 0,                                   /* 0xa - 0xb      */
        0,                                      /* 0xc            */
        0,                                      /* 0xd            */
        0,                                      /* 0xe            */
        1                                       /* 0xf            */
    }
 },

/* IXbrick widget number to PCI bus number map */
 {      MODULE_IXBRICK,                         /* IXbrick type   */ 
    /*  PCI Bus #                                  Widget #       */
    {   0, 0, 0, 0, 0, 0, 0, 0,                 /* 0x0 - 0x7      */
        0,                                      /* 0x8            */
        0,                                      /* 0x9            */
        0, 0,                                   /* 0xa - 0xb      */
        1,                                      /* 0xc            */
        5,                                      /* 0xd            */
        0,                                      /* 0xe            */
        3                                       /* 0xf            */
    }
 },

/* Xbrick widget to XIO slot map */
 {      MODULE_XBRICK,                          /* Xbrick type    */ 
    /*  XIO Slot #                                 Widget #       */
    {   0, 0, 0, 0, 0, 0, 0, 0,                 /* 0x0 - 0x7      */
        1,                                      /* 0x8            */
        3,                                      /* 0x9            */
        0, 0,                                   /* 0xa - 0xb      */
        2,                                      /* 0xc            */
        4,                                      /* 0xd            */
        0,                                      /* 0xe            */
        0                                       /* 0xf            */
    }
 },
/* CG brick widget number to PCI bus number map */
 {      MODULE_CGBRICK,                    /* CG brick       */
    /*  PCI Bus #                                  Widget #       */
    {   0, 0, 0, 0, 0, 0, 0, 0,                 /* 0x0 - 0x7      */
        0,                                      /* 0x8            */
        0,                                      /* 0x9            */
        0, 1,                                   /* 0xa - 0xb      */
        0,                                      /* 0xc            */
        0,                                      /* 0xd            */
        0,                                      /* 0xe            */
        0                                       /* 0xf            */
     }
 }
};

/*
 * Use the brick's type to map a widget number to a meaningful int
 */
int
io_brick_map_widget(int brick_type, int widget_num)
{
        int num_bricks, i;

        /* Calculate number of bricks in table */
        num_bricks = sizeof(io_brick_tab)/sizeof(io_brick_tab[0]);

        /* Look for brick prefix in table */
        for (i = 0; i < num_bricks; i++) {
               if (brick_type == io_brick_tab[i].ibm_type)
                       return(io_brick_tab[i].ibm_map_wid[widget_num]);
        }

        return 0;

}
