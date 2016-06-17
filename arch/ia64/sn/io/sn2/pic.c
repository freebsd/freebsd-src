/*
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2001-2003 Silicon Graphics, Inc. All rights reserved.
 */

#include <linux/types.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <asm/sn/sgi.h>
#include <asm/sn/sn_cpuid.h>
#include <asm/sn/addrs.h>
#include <asm/sn/arch.h>
#include <asm/sn/iograph.h>
#include <asm/sn/invent.h>
#include <asm/sn/hcl.h>
#include <asm/sn/hcl_util.h>
#include <asm/sn/labelcl.h>
#include <asm/sn/xtalk/xwidget.h>
#include <asm/sn/pci/bridge.h>
#include <asm/sn/pci/pciio.h>
#include <asm/sn/pci/pcibr.h>
#include <asm/sn/pci/pcibr_private.h>
#include <asm/sn/pci/pci_defs.h>
#include <asm/sn/prio.h>
#include <asm/sn/xtalk/xbow.h>
#include <asm/sn/ioc3.h>
#include <asm/sn/io.h>
#include <asm/sn/sn_private.h>

extern char *bcopy(const char * src, char * dest, int count);


#define PCI_BUS_NO_1 1

extern int pcibr_attach2(vertex_hdl_t, bridge_t *, vertex_hdl_t, int, pcibr_soft_t *);
extern void pcibr_driver_reg_callback(vertex_hdl_t, int, int, int);
extern void pcibr_driver_unreg_callback(vertex_hdl_t, int, int, int);


/*
 * copy inventory_t from conn_v to peer_conn_v
 */
int
pic_bus1_inventory_dup(vertex_hdl_t conn_v, vertex_hdl_t peer_conn_v)
{
	inventory_t *pinv, *peer_pinv;

	if (hwgraph_info_get_LBL(conn_v, INFO_LBL_INVENT,
				(arbitrary_info_t *)&pinv) == GRAPH_SUCCESS)
 {
		NEW(peer_pinv);
		bcopy((const char *)pinv, (char *)peer_pinv, sizeof(inventory_t));
		if (hwgraph_info_add_LBL(peer_conn_v, INFO_LBL_INVENT,
			    (arbitrary_info_t)peer_pinv) != GRAPH_SUCCESS) {
			DEL(peer_pinv);
			return 0;
		}
		return 1;
	}

	printk("pic_bus1_inventory_dup: cannot get INFO_LBL_INVENT from 0x%lx\n ", (uint64_t)conn_v);
	return 0;
}

/*
 * copy xwidget_info_t from conn_v to peer_conn_v
 */
int
pic_bus1_widget_info_dup(vertex_hdl_t conn_v, vertex_hdl_t peer_conn_v,
							cnodeid_t xbow_peer)
{
	xwidget_info_t widget_info, peer_widget_info;
	char peer_path[256];
	vertex_hdl_t peer_hubv;
	hubinfo_t peer_hub_info;

	/* get the peer hub's widgetid */
	peer_hubv = NODEPDA(xbow_peer)->node_vertex;
	peer_hub_info = NULL;
	hubinfo_get(peer_hubv, &peer_hub_info);
	if (peer_hub_info == NULL)
		return 0;

	if (hwgraph_info_get_LBL(conn_v, INFO_LBL_XWIDGET,
			(arbitrary_info_t *)&widget_info) == GRAPH_SUCCESS) {
		NEW(peer_widget_info);
		peer_widget_info->w_vertex = peer_conn_v;
		peer_widget_info->w_id = widget_info->w_id;
		peer_widget_info->w_master = peer_hubv;
		peer_widget_info->w_masterid = peer_hub_info->h_widgetid;
		/* structure copy */
		peer_widget_info->w_hwid = widget_info->w_hwid;
		peer_widget_info->w_efunc = 0;
		peer_widget_info->w_einfo = 0;
		peer_widget_info->w_name = kmalloc(strlen(peer_path) + 1, GFP_KERNEL);
		strcpy(peer_widget_info->w_name, peer_path);

		if (hwgraph_info_add_LBL(peer_conn_v, INFO_LBL_XWIDGET,
			(arbitrary_info_t)peer_widget_info) != GRAPH_SUCCESS) {
				DEL(peer_widget_info);
				return 0;
		}

		xwidget_info_set(peer_conn_v, peer_widget_info);

		return 1;
	}

	printk("pic_bus1_widget_info_dup: "
			"cannot get INFO_LBL_XWIDGET from 0x%lx\n", (uint64_t)conn_v);
	return 0;
}

/*
 * If this PIC is attached to two Cbricks ("dual-ported") then
 * attach each bus to opposite Cbricks.
 *
 * If successful, return a new vertex suitable for attaching the PIC bus.
 * If not successful, return zero and both buses will attach to the
 * vertex passed into pic_attach().
 */
vertex_hdl_t
pic_bus1_redist(nasid_t nasid, vertex_hdl_t conn_v)
{
	cnodeid_t cnode = NASID_TO_COMPACT_NODEID(nasid);
	cnodeid_t xbow_peer = -1;
	char pathname[256], peer_path[256], tmpbuf[256];
	char *p;
	int rc;
	vertex_hdl_t peer_conn_v, hubv;
	int pos;
	slabid_t slab;

	if (NODEPDA(cnode)->xbow_peer >= 0) {			/* if dual-ported */
		/* create a path for this widget on the peer Cbrick */
		/* pcibr widget hw/module/001c11/slab/0/Pbrick/xtalk/12 */
		/* sprintf(pathname, "%v", conn_v); */
		xbow_peer = NASID_TO_COMPACT_NODEID(NODEPDA(cnode)->xbow_peer);
		pos = hwgraph_generate_path(conn_v, tmpbuf, 256);
		strcpy(pathname, &tmpbuf[pos]);
		p = pathname + strlen("hw/module/001c01/slab/0/");

		memset(tmpbuf, 0, 16);
		format_module_id(tmpbuf, geo_module((NODEPDA(xbow_peer))->geoid), MODULE_FORMAT_BRIEF);
		slab = geo_slab((NODEPDA(xbow_peer))->geoid);
		sprintf(peer_path, "module/%s/slab/%d/%s", tmpbuf, (int)slab, p); 
		
		/* Look for vertex for this widget on the peer Cbrick.
		 * Expect GRAPH_NOT_FOUND.
		 */
		rc = hwgraph_traverse(hwgraph_root, peer_path, &peer_conn_v);
		if (GRAPH_SUCCESS == rc)
			printk("pic_attach: found unexpected vertex: 0x%lx\n",
								(uint64_t)peer_conn_v);
		else if (GRAPH_NOT_FOUND != rc) {
			printk("pic_attach: hwgraph_traverse unexpectedly"
					" returned 0x%x\n", rc);
		} else {
			/* try to add the widget vertex to the peer Cbrick */
			rc = hwgraph_path_add(hwgraph_root, peer_path, &peer_conn_v);

			if (GRAPH_SUCCESS != rc)
			    printk("pic_attach: hwgraph_path_add"
						" failed with 0x%x\n", rc);
			else {
			    PCIBR_DEBUG_ALWAYS((PCIBR_DEBUG_ATTACH, conn_v,
					"pic_bus1_redist: added vertex %v\n", peer_conn_v)); 

			    /* Now hang appropiate stuff off of the new
			     * vertex.	We bail out if we cannot add something.
			     * In that case, we don't remove the newly added
			     * vertex but that should be safe and we don't
			     * really expect the additions to fail anyway.
			     */
#if 0
			    if (!pic_bus1_inventory_dup(conn_v, peer_conn_v))
					return 0;
			    pic_bus1_device_desc_dup(conn_v, peer_conn_v);
#endif
			    if (!pic_bus1_widget_info_dup(conn_v, peer_conn_v, xbow_peer))
					return 0;

			    hubv = cnodeid_to_vertex(xbow_peer);
			    ASSERT(hubv != GRAPH_VERTEX_NONE);
			    device_master_set(peer_conn_v, hubv);
			    xtalk_provider_register(hubv, &hub_provider);
			    xtalk_provider_startup(hubv);
			    return peer_conn_v;
			}
		}
	}
	return 0;
}


int
pic_attach(vertex_hdl_t conn_v)
{
	int		rc;
	bridge_t	*bridge0, *bridge1 = (bridge_t *)0;
	vertex_hdl_t	pcibr_vhdl0, pcibr_vhdl1 = (vertex_hdl_t)0;
	pcibr_soft_t	bus0_soft, bus1_soft = (pcibr_soft_t)0;
	vertex_hdl_t  conn_v0, conn_v1, peer_conn_v;
	int             brick_type;
	int             iobrick_type_get_nasid(nasid_t nasid);


	PCIBR_DEBUG_ALWAYS((PCIBR_DEBUG_ATTACH, conn_v, "pic_attach()\n"));

	bridge0 = (bridge_t *) xtalk_piotrans_addr(conn_v, NULL,
	                        0, sizeof(bridge_t), 0);
	bridge1 = (bridge_t *)((char *)bridge0 + PIC_BUS1_OFFSET);

	PCIBR_DEBUG_ALWAYS((PCIBR_DEBUG_ATTACH, conn_v,
		    "pic_attach: bridge0=0x%x, bridge1=0x%x\n", 
		    bridge0, bridge1));

	conn_v0 = conn_v1 = conn_v;

	/* If dual-ported then split the two PIC buses across both Cbricks */
	if ((peer_conn_v = (pic_bus1_redist(NASID_GET(bridge0), conn_v))))
		conn_v1 = peer_conn_v;

	/*
	 * Create the vertex for the PCI buses, which we
	 * will also use to hold the pcibr_soft and
	 * which will be the "master" vertex for all the
	 * pciio connection points we will hang off it.
	 * This needs to happen before we call nic_bridge_vertex_info
	 * as we are some of the *_vmc functions need access to the edges.
	 *
	 * Opening this vertex will provide access to
	 * the Bridge registers themselves.
	 */
	/* FIXME: what should the hwgraph path look like ? */
	brick_type = iobrick_type_get_nasid(NASID_GET(bridge0));
	if ( brick_type == MODULE_CGBRICK ) {
		rc = hwgraph_path_add(conn_v0, EDGE_LBL_AGP_0, &pcibr_vhdl0);
		ASSERT(rc == GRAPH_SUCCESS);
		rc = hwgraph_path_add(conn_v1, EDGE_LBL_AGP_1, &pcibr_vhdl1);
		ASSERT(rc == GRAPH_SUCCESS);
	}
	else {
		rc = hwgraph_path_add(conn_v0, EDGE_LBL_PCIX_0, &pcibr_vhdl0);
		ASSERT(rc == GRAPH_SUCCESS);
		rc = hwgraph_path_add(conn_v1, EDGE_LBL_PCIX_1, &pcibr_vhdl1);
		ASSERT(rc == GRAPH_SUCCESS);
	}

	PCIBR_DEBUG_ALWAYS((PCIBR_DEBUG_ATTACH, conn_v,
		    "pic_attach: pcibr_vhdl0=%v, pcibr_vhdl1=%v\n",
		    pcibr_vhdl0, pcibr_vhdl1));

	/* register pci provider array */
	pciio_provider_register(pcibr_vhdl0, &pci_pic_provider);
	pciio_provider_register(pcibr_vhdl1, &pci_pic_provider);

	pciio_provider_startup(pcibr_vhdl0);
	pciio_provider_startup(pcibr_vhdl1);

	pcibr_attach2(conn_v0, bridge0, pcibr_vhdl0, 0, &bus0_soft);
	pcibr_attach2(conn_v1, bridge1, pcibr_vhdl1, 1, &bus1_soft);

	/* save a pointer to the PIC's other bus's soft struct */
        bus0_soft->bs_peers_soft = bus1_soft;
        bus1_soft->bs_peers_soft = bus0_soft;

	PCIBR_DEBUG_ALWAYS((PCIBR_DEBUG_ATTACH, conn_v,
		    "pic_attach: bus0_soft=0x%x, bus1_soft=0x%x\n",
		    bus0_soft, bus1_soft));

	return 0;
}

/*
 * pci provider functions
 *
 * mostly in pcibr.c but if any are needed here then
 * this might be a way to get them here.
 */
pciio_provider_t        pci_pic_provider =
{
    (pciio_piomap_alloc_f *) pcibr_piomap_alloc,
    (pciio_piomap_free_f *) pcibr_piomap_free,
    (pciio_piomap_addr_f *) pcibr_piomap_addr,
    (pciio_piomap_done_f *) pcibr_piomap_done,
    (pciio_piotrans_addr_f *) pcibr_piotrans_addr,
    (pciio_piospace_alloc_f *) pcibr_piospace_alloc,
    (pciio_piospace_free_f *) pcibr_piospace_free,

    (pciio_dmamap_alloc_f *) pcibr_dmamap_alloc,
    (pciio_dmamap_free_f *) pcibr_dmamap_free,
    (pciio_dmamap_addr_f *) pcibr_dmamap_addr,
    (pciio_dmamap_done_f *) pcibr_dmamap_done,
    (pciio_dmatrans_addr_f *) pcibr_dmatrans_addr,
    (pciio_dmamap_drain_f *) pcibr_dmamap_drain,
    (pciio_dmaaddr_drain_f *) pcibr_dmaaddr_drain,
    (pciio_dmalist_drain_f *) pcibr_dmalist_drain,

    (pciio_intr_alloc_f *) pcibr_intr_alloc,
    (pciio_intr_free_f *) pcibr_intr_free,
    (pciio_intr_connect_f *) pcibr_intr_connect,
    (pciio_intr_disconnect_f *) pcibr_intr_disconnect,
    (pciio_intr_cpu_get_f *) pcibr_intr_cpu_get,

    (pciio_provider_startup_f *) pcibr_provider_startup,
    (pciio_provider_shutdown_f *) pcibr_provider_shutdown,
    (pciio_reset_f *) pcibr_reset,
    (pciio_endian_set_f *) pcibr_endian_set,
    (pciio_config_get_f *) pcibr_config_get,
    (pciio_config_set_f *) pcibr_config_set,
    (pciio_error_devenable_f *) 0,
    (pciio_error_extract_f *) 0,
    (pciio_driver_reg_callback_f *) pcibr_driver_reg_callback,
    (pciio_driver_unreg_callback_f *) pcibr_driver_unreg_callback,
    (pciio_device_unregister_f 	*) pcibr_device_unregister,
    (pciio_dma_enabled_f		*) pcibr_dma_enabled,
};
