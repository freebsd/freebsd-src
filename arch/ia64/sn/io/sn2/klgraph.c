/* $Id$
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1992 - 1997, 2000-2003 Silicon Graphics, Inc. All rights reserved.
 */

/*
 * klgraph.c-
 *      This file specifies the interface between the kernel and the PROM's
 *      configuration data structures.
 */

#include <linux/types.h>
#include <linux/slab.h>
#include <asm/sn/sgi.h>
#include <asm/sn/sn_sal.h>
#include <asm/sn/io.h>
#include <asm/sn/iograph.h>
#include <asm/sn/invent.h>
#include <asm/sn/hcl.h>
#include <asm/sn/labelcl.h>
#include <asm/sn/kldir.h>
#include <asm/sn/klconfig.h>
#include <asm/sn/router.h>
#include <asm/sn/xtalk/xbow.h>
#include <asm/sn/hcl_util.h>

// #define KLGRAPH_DEBUG 1
#ifdef KLGRAPH_DEBUG
#define GRPRINTF(x)	printk x
#define CE_GRPANIC	CE_PANIC
#else
#define GRPRINTF(x)
#define CE_GRPANIC	CE_PANIC
#endif

#include <asm/sn/sn_private.h>

extern char arg_maxnodes[];
extern u64 klgraph_addr[];
void mark_cpuvertex_as_cpu(vertex_hdl_t vhdl, cpuid_t cpuid);
extern int is_specified(char *);


/*
 * Support for verbose inventory via hardware graph. 
 * klhwg_invent_alloc allocates the necessary size of inventory information
 * and fills in the generic information.
 */
invent_generic_t *
klhwg_invent_alloc(cnodeid_t cnode, int class, int size)
{
	invent_generic_t *invent;

	invent = kmalloc(size, GFP_KERNEL);
	if (!invent) return NULL;
	
	invent->ig_module = NODE_MODULEID(cnode);
	invent->ig_slot = SLOTNUM_GETSLOT(NODE_SLOTID(cnode));
	invent->ig_invclass = class;

	return invent;
}

/*
 * Add detailed disabled cpu inventory info to the hardware graph.
 */
void
klhwg_disabled_cpu_invent_info(vertex_hdl_t cpuv,
                               cnodeid_t cnode,
                               klcpu_t *cpu, slotid_t slot)
{
	invent_cpuinfo_t *cpu_invent;
	diag_inv_t       *diag_invent;

	cpu_invent = (invent_cpuinfo_t *)
	klhwg_invent_alloc(cnode, INV_PROCESSOR, sizeof(invent_cpuinfo_t));
	if (!cpu_invent)
		return;

	/* Diag information on this processor */
	diag_invent = (diag_inv_t *)
	klhwg_invent_alloc(cnode, INV_CPUDIAGVAL, sizeof(diag_inv_t));

	if (!diag_invent)
		return;


	/* Disabled CPU */
	cpu_invent->ic_gen.ig_flag = 0x0;
	cpu_invent->ic_gen.ig_slot = slot;
	cpu_invent->ic_cpu_info.cpuflavor = cpu->cpu_prid;
	cpu_invent->ic_cpu_info.cpufq = cpu->cpu_speed;
	cpu_invent->ic_cpu_info.sdfreq = cpu->cpu_scachespeed;

	cpu_invent->ic_cpu_info.sdsize = cpu->cpu_scachesz;
	cpu_invent->ic_cpuid = cpu->cpu_info.virtid;
	cpu_invent->ic_slice = cpu->cpu_info.physid;

	/* Disabled CPU label */
	hwgraph_info_add_LBL(cpuv, INFO_LBL_DETAIL_INVENT,
			(arbitrary_info_t) cpu_invent);
	hwgraph_info_export_LBL(cpuv, INFO_LBL_DETAIL_INVENT,
			sizeof(invent_cpuinfo_t));

	/* Diagval label - stores reason for disable +{virt,phys}id +diagval*/
	hwgraph_info_add_LBL(cpuv, INFO_LBL_DIAGVAL,
			(arbitrary_info_t) diag_invent);

	hwgraph_info_export_LBL(cpuv, INFO_LBL_DIAGVAL,
			sizeof(diag_inv_t));
}

/*
 * Add detailed cpu inventory info to the hardware graph.
 */
void
klhwg_cpu_invent_info(vertex_hdl_t cpuv,
			cnodeid_t cnode,
			klcpu_t *cpu)
{
	invent_cpuinfo_t *cpu_invent;

	cpu_invent = (invent_cpuinfo_t *)
		klhwg_invent_alloc(cnode, INV_PROCESSOR, sizeof(invent_cpuinfo_t));
	if (!cpu_invent)
		return;

	if (KLCONFIG_INFO_ENABLED((klinfo_t *)cpu))
		cpu_invent->ic_gen.ig_flag = INVENT_ENABLED;
	else
		cpu_invent->ic_gen.ig_flag = 0x0;

	cpu_invent->ic_cpu_info.cpuflavor = cpu->cpu_prid;
	cpu_invent->ic_cpu_info.cpufq = cpu->cpu_speed;
	cpu_invent->ic_cpu_info.sdfreq = cpu->cpu_scachespeed;

	cpu_invent->ic_cpu_info.sdsize = cpu->cpu_scachesz;
	cpu_invent->ic_cpuid = cpu->cpu_info.virtid;
	cpu_invent->ic_slice = cpu_physical_id_to_slice(cpu->cpu_info.virtid);

	hwgraph_info_add_LBL(cpuv, INFO_LBL_DETAIL_INVENT,
			(arbitrary_info_t) cpu_invent);
	hwgraph_info_export_LBL(cpuv, INFO_LBL_DETAIL_INVENT,
			sizeof(invent_cpuinfo_t));
}

/* 
 * Add information about the baseio prom version number
 * as a part of detailed inventory info in the hwgraph.
 */
void
klhwg_baseio_inventory_add(vertex_hdl_t baseio_vhdl,cnodeid_t cnode)
{
	invent_miscinfo_t	*baseio_inventory;
	unsigned char		version = 0,revision = 0;

	/* Allocate memory for the "detailed inventory" info
	 * for the baseio
	 */
	baseio_inventory = (invent_miscinfo_t *) 
		klhwg_invent_alloc(cnode, INV_PROM, sizeof(invent_miscinfo_t));
	baseio_inventory->im_type = INV_IO6PROM;
	/* Store the revision info  in the inventory */
	baseio_inventory->im_version = version;
	baseio_inventory->im_rev = revision;
	/* Put the inventory info in the hardware graph */
	hwgraph_info_add_LBL(baseio_vhdl, INFO_LBL_DETAIL_INVENT, 
			     (arbitrary_info_t) baseio_inventory);
	/* Make the information available to the user programs
	 * thru hwgfs.
	 */
        hwgraph_info_export_LBL(baseio_vhdl, INFO_LBL_DETAIL_INVENT,
				sizeof(invent_miscinfo_t));
}

/*
 * Add detailed cpu inventory info to the hardware graph.
 */
void
klhwg_hub_invent_info(vertex_hdl_t hubv,
		      cnodeid_t cnode, 
		      klhub_t *hub)
{
	invent_miscinfo_t *hub_invent;

	hub_invent = (invent_miscinfo_t *) 
	    klhwg_invent_alloc(cnode, INV_MISC, sizeof(invent_miscinfo_t));
	if (!hub_invent)
	    return;

	if (KLCONFIG_INFO_ENABLED((klinfo_t *)hub))
	    hub_invent->im_gen.ig_flag = INVENT_ENABLED;

	hub_invent->im_type = INV_HUB;
	hub_invent->im_rev = hub->hub_info.revision;
	hub_invent->im_speed = hub->hub_speed;
	hwgraph_info_add_LBL(hubv, INFO_LBL_DETAIL_INVENT, 
			     (arbitrary_info_t) hub_invent);
        hwgraph_info_export_LBL(hubv, INFO_LBL_DETAIL_INVENT,
				sizeof(invent_miscinfo_t));
}

/* ARGSUSED */
void
klhwg_add_ice(vertex_hdl_t node_vertex, klhub_t *hub, cnodeid_t cnode)
{
	vertex_hdl_t myicev;
	vertex_hdl_t ice_mon;
	int rc;
	extern struct file_operations shub_mon_fops;

	(void) hwgraph_path_add(node_vertex, EDGE_LBL_ICE, &myicev);

	HWGRAPH_DEBUG((__FILE__, __FUNCTION__, __LINE__, myicev, NULL, "Created path for ice vertex for TIO node.\n"));

	rc = device_master_set(myicev, node_vertex);
	if (rc)
		panic("klhwg_add_ice: Unable to create ice vertex.\n");

	ice_mon = hwgraph_register(myicev, EDGE_LBL_PERFMON,
		0, DEVFS_FL_AUTO_DEVNUM,
		0, 0,
		S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP, 0, 0,
		&shub_mon_fops, (void *)(long)cnode);
}

/* ARGSUSED */
void
klhwg_add_hub(vertex_hdl_t node_vertex, klhub_t *hub, cnodeid_t cnode)
{
	vertex_hdl_t myhubv;
	vertex_hdl_t hub_mon;
	int rc;
	extern struct file_operations shub_mon_fops;

	GRPRINTF(("klhwg_add_hub: adding %s\n", EDGE_LBL_HUB));
	(void) hwgraph_path_add(node_vertex, EDGE_LBL_HUB, &myhubv);

	HWGRAPH_DEBUG((__FILE__, __FUNCTION__,__LINE__, myhubv, NULL, "Created path for hub vertex for Shub node.\n"));

	rc = device_master_set(myhubv, node_vertex);
	if (rc)
		panic("klhwg_add_hub: Unable to create hub vertex.\n");

	hub_mon = hwgraph_register(myhubv, EDGE_LBL_PERFMON,
		0, DEVFS_FL_AUTO_DEVNUM,
		0, 0,
		S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP, 0, 0,
		&shub_mon_fops, (void *)(long)cnode);
}

/* ARGSUSED */
void
klhwg_add_disabled_cpu(vertex_hdl_t node_vertex, cnodeid_t cnode, klcpu_t *cpu, slotid_t slot)
{
        vertex_hdl_t my_cpu;
        char name[120];
        cpuid_t cpu_id;
	nasid_t nasid;

	nasid = COMPACT_TO_NASID_NODEID(cnode);
        cpu_id = nasid_slice_to_cpuid(nasid, cpu->cpu_info.physid);
        if(cpu_id != -1){
		sprintf(name, "%s/%s/%c", EDGE_LBL_DISABLED, EDGE_LBL_CPU, 'a' + cpu->cpu_info.physid);
		(void) hwgraph_path_add(node_vertex, name, &my_cpu);

		HWGRAPH_DEBUG((__FILE__, __FUNCTION__,__LINE__, my_cpu, NULL, "Created path for disabled cpu slice.\n"));

		mark_cpuvertex_as_cpu(my_cpu, cpu_id);
		device_master_set(my_cpu, node_vertex);

		klhwg_disabled_cpu_invent_info(my_cpu, cnode, cpu, slot);
		return;
        }
}

/* ARGSUSED */
void
klhwg_add_cpu(vertex_hdl_t node_vertex, cnodeid_t cnode, klcpu_t *cpu)
{
        vertex_hdl_t my_cpu, cpu_dir;
        char name[120];
        cpuid_t cpu_id;
	nasid_t nasid;

	nasid = COMPACT_TO_NASID_NODEID(cnode);
        cpu_id = nasid_slice_to_cpuid(nasid, cpu->cpu_info.physid);

        sprintf(name, "%s/%d/%c",
                EDGE_LBL_CPUBUS,
                0,
                'a' + cpu->cpu_info.physid);

        GRPRINTF(("klhwg_add_cpu: adding %s to vertex 0x%p\n", name, node_vertex));
        (void) hwgraph_path_add(node_vertex, name, &my_cpu);

	HWGRAPH_DEBUG((__FILE__, __FUNCTION__,__LINE__, my_cpu, NULL, "Created path for active cpu slice.\n"));

        mark_cpuvertex_as_cpu(my_cpu, cpu_id);
        device_master_set(my_cpu, node_vertex);

        /* Add an alias under the node's CPU directory */
        if (hwgraph_edge_get(node_vertex, EDGE_LBL_CPU, &cpu_dir) == GRAPH_SUCCESS) {
                sprintf(name, "%c", 'a' + cpu->cpu_info.physid);
                (void) hwgraph_edge_add(cpu_dir, my_cpu, name);
		HWGRAPH_DEBUG((__FILE__, __FUNCTION__,__LINE__, cpu_dir, my_cpu, "Created % from vhdl1 to vhdl2.\n", name));
        }

        klhwg_cpu_invent_info(my_cpu, cnode, cpu);
}


void
klhwg_add_coretalk(cnodeid_t cnode, nasid_t tio_nasid)
{
	lboard_t *brd;
	vertex_hdl_t coretalk_v, icev;
	/*REFERENCED*/
	graph_error_t err;

	if ((brd = find_lboard((lboard_t *)KL_CONFIG_INFO(tio_nasid), KLTYPE_IOBRICK_XBOW)) == NULL)
			return;

	if (KL_CONFIG_DUPLICATE_BOARD(brd))
	    return;

	icev = cnodeid_to_vertex(cnode);

	err = hwgraph_path_add(icev, EDGE_LBL_CORETALK, &coretalk_v);
	if (err != GRAPH_SUCCESS) {
		if (err == GRAPH_DUP)
			printk(KERN_WARNING  "klhwg_add_coretalk: Check for "
                                        "working routers and router links!");

                 panic("klhwg_add_coretalkk: Failed to add "
                             "edge: vertex 0x%p to vertex 0x%p,"
                             "error %d\n",
                              (void *)icev, (void *)coretalk_v, err);
        }

	HWGRAPH_DEBUG((__FILE__, __FUNCTION__, __LINE__, coretalk_v, NULL, "Created coretalk path for TIO node.\n"));

        NODEPDA(cnode)->xbow_vhdl = coretalk_v;

}


void
klhwg_add_xbow(cnodeid_t cnode, nasid_t nasid)
{
	lboard_t *brd;
	klxbow_t *xbow_p;
	nasid_t hub_nasid;
	cnodeid_t hub_cnode;
	int widgetnum;
	vertex_hdl_t xbow_v, hubv;
	/*REFERENCED*/
	graph_error_t err;

	if ((brd = find_lboard((lboard_t *)KL_CONFIG_INFO(nasid), KLTYPE_IOBRICK_XBOW)) == NULL)
			return;

	if (KL_CONFIG_DUPLICATE_BOARD(brd))
	    return;

	if ((xbow_p = (klxbow_t *)find_component(brd, NULL, KLSTRUCT_XBOW))
	    == NULL)
	    return;

	for (widgetnum = HUB_WIDGET_ID_MIN; widgetnum <= HUB_WIDGET_ID_MAX; widgetnum++) {
		if (!XBOW_PORT_TYPE_HUB(xbow_p, widgetnum)) 
		    continue;

		hub_nasid = XBOW_PORT_NASID(xbow_p, widgetnum);
		if (hub_nasid == INVALID_NASID) {
			printk(KERN_WARNING  "hub widget %d, skipping xbow graph\n", widgetnum);
			continue;
		}

		hub_cnode = NASID_TO_COMPACT_NODEID(hub_nasid);

		if (is_specified(arg_maxnodes) && hub_cnode == INVALID_CNODEID) {
			continue;
		}
			
		hubv = cnodeid_to_vertex(hub_cnode);

		err = hwgraph_path_add(hubv, EDGE_LBL_XTALK, &xbow_v);
                if (err != GRAPH_SUCCESS) {
                        if (err == GRAPH_DUP)
                                printk(KERN_WARNING  "klhwg_add_xbow: Check for "
                                        "working routers and router links!");

                        panic("klhwg_add_xbow: Failed to add "
                                "edge: vertex 0x%p to vertex 0x%p,"
                                "error %d\n",
                                (void *)hubv, (void *)xbow_v, err);
                }

		HWGRAPH_DEBUG((__FILE__, __FUNCTION__, __LINE__, xbow_v, NULL, "Created path for xtalk.\n"));

		xswitch_vertex_init(xbow_v); 

		NODEPDA(hub_cnode)->xbow_vhdl = xbow_v;

		/*
		 * XXX - This won't work is we ever hook up two hubs
		 * by crosstown through a crossbow.
		 */
		if (hub_nasid != nasid) {
			NODEPDA(hub_cnode)->xbow_peer = nasid;
			NODEPDA(NASID_TO_COMPACT_NODEID(nasid))->xbow_peer =
				hub_nasid;
		}

	}
}


/* ARGSUSED */
void
klhwg_add_tionode(vertex_hdl_t hwgraph_root, cnodeid_t cnode)
{
	nasid_t tio_nasid;
	lboard_t *brd;
	klhub_t *hub;
	vertex_hdl_t node_vertex = NULL;
	char path_buffer[100];
	int rv;
	char *s;
	int board_disabled = 0;

	tio_nasid = COMPACT_TO_NASID_NODEID(cnode);
	brd = find_lboard((lboard_t *)KL_CONFIG_INFO(tio_nasid), KLTYPE_TIO);
	ASSERT(brd);

	/* Generate a hardware graph path for this board. */
	board_to_path(brd, path_buffer);
	rv = hwgraph_path_add(hwgraph_root, path_buffer, &node_vertex);
	if (rv != GRAPH_SUCCESS)
		panic("TIO Node vertex creation failed.  "
					  "Path == %s", path_buffer);

	HWGRAPH_DEBUG((__FILE__, __FUNCTION__, __LINE__, node_vertex, NULL, "Created path for TIO node.\n"));
	hub = (klhub_t *)find_first_component(brd, KLSTRUCT_HUB);
	ASSERT(hub);
	if(hub->hub_info.flags & KLINFO_ENABLE)
		board_disabled = 0;
	else
		board_disabled = 1;

	if(!board_disabled) {
		mark_nodevertex_as_node(node_vertex,
				    cnode + board_disabled * numionodes);

		s = dev_to_name(node_vertex, path_buffer, sizeof(path_buffer));
		NODEPDA(cnode)->hwg_node_name =
					kmalloc(strlen(s) + 1,
					GFP_KERNEL);
		ASSERT_ALWAYS(NODEPDA(cnode)->hwg_node_name != NULL);
		strcpy(NODEPDA(cnode)->hwg_node_name, s);

		hubinfo_set(node_vertex, NODEPDA(cnode)->pdinfo);

		/* Set up node board's slot */
		NODEPDA(cnode)->slotdesc = brd->brd_slot;

		/* Set up the module we're in */
		NODEPDA(cnode)->geoid = brd->brd_geoid;
		NODEPDA(cnode)->module = module_lookup(geo_module(brd->brd_geoid));
	}

        if(!board_disabled)
                klhwg_add_ice(node_vertex, hub, cnode);

}


/* ARGSUSED */
void
klhwg_add_node(vertex_hdl_t hwgraph_root, cnodeid_t cnode)
{
	nasid_t nasid;
	lboard_t *brd;
	klhub_t *hub;
	vertex_hdl_t node_vertex = NULL;
	char path_buffer[100];
	int rv;
	char *s;
	int board_disabled = 0;
	klcpu_t *cpu;

	nasid = COMPACT_TO_NASID_NODEID(cnode);
	brd = find_lboard((lboard_t *)KL_CONFIG_INFO(nasid), KLTYPE_SNIA);
	ASSERT(brd);

	do {
		vertex_hdl_t cpu_dir;

		/* Generate a hardware graph path for this board. */
		board_to_path(brd, path_buffer);
		rv = hwgraph_path_add(hwgraph_root, path_buffer, &node_vertex);
		if (rv != GRAPH_SUCCESS)
			panic("Node vertex creation failed.  Path == %s", path_buffer);

		HWGRAPH_DEBUG((__FILE__, __FUNCTION__, __LINE__, node_vertex, NULL, "Created path for SHUB node.\n"));
		hub = (klhub_t *)find_first_component(brd, KLSTRUCT_HUB);
		ASSERT(hub);
		if(hub->hub_info.flags & KLINFO_ENABLE)
			board_disabled = 0;
		else
			board_disabled = 1;
		
		if(!board_disabled) {
			mark_nodevertex_as_node(node_vertex,
					    cnode + board_disabled * numnodes);

			s = dev_to_name(node_vertex, path_buffer, sizeof(path_buffer));
			NODEPDA(cnode)->hwg_node_name =
						kmalloc(strlen(s) + 1,
						GFP_KERNEL);
			ASSERT_ALWAYS(NODEPDA(cnode)->hwg_node_name != NULL);
			strcpy(NODEPDA(cnode)->hwg_node_name, s);

			hubinfo_set(node_vertex, NODEPDA(cnode)->pdinfo);

			/* Set up node board's slot */
			NODEPDA(cnode)->slotdesc = brd->brd_slot;

			/* Set up the module we're in */
			NODEPDA(cnode)->geoid = brd->brd_geoid;
			NODEPDA(cnode)->module = module_lookup(geo_module(brd->brd_geoid));
		}

		/* Get the first CPU structure */
		cpu = (klcpu_t *)find_first_component(brd, KLSTRUCT_CPU);

		/*
		* If there's at least 1 CPU, add a "cpu" directory to represent
		* the collection of all CPUs attached to this node.
		*/
		if (cpu) {
			graph_error_t rv;

			rv = hwgraph_path_add(node_vertex, EDGE_LBL_CPU, &cpu_dir);
			if (rv != GRAPH_SUCCESS)
				panic("klhwg_add_node: Cannot create CPU directory\n");
			HWGRAPH_DEBUG((__FILE__, __FUNCTION__, __LINE__, cpu_dir, NULL, "Created cpu directiry on SHUB node.\n"));

		}

		/* Add each CPU */
		while (cpu) {
			cpuid_t cpu_id;
			cpu_id = nasid_slice_to_cpuid(nasid,cpu->cpu_info.physid);
			if (cpu_online(cpu_id))
				klhwg_add_cpu(node_vertex, cnode, cpu);
			else
				klhwg_add_disabled_cpu(node_vertex, cnode, cpu, brd->brd_slot);

			cpu = (klcpu_t *)
				find_component(brd, (klinfo_t *)cpu, KLSTRUCT_CPU);
		} /* while */

		if(!board_disabled)
			klhwg_add_hub(node_vertex, hub, cnode);
		
		brd = KLCF_NEXT(brd);
		if (brd)
			brd = find_lboard(brd, KLTYPE_SNIA);
		else
			break;
	} while(brd);
}


/* ARGSUSED */
void
klhwg_add_all_routers(vertex_hdl_t hwgraph_root)
{
	nasid_t nasid;
	cnodeid_t cnode;
	lboard_t *brd;
	vertex_hdl_t node_vertex;
	char path_buffer[100];
	int rv;

	for (cnode = 0; cnode < numnodes; cnode++) {
		nasid = COMPACT_TO_NASID_NODEID(cnode);
		brd = find_lboard_class((lboard_t *)KL_CONFIG_INFO(nasid),
				KLTYPE_ROUTER);
		if (!brd)
			/* No routers stored in this node's memory */
			continue;

		do {
			ASSERT(brd);

			/* Don't add duplicate boards. */
			if (brd->brd_flags & DUPLICATE_BOARD)
				continue;

			/* Generate a hardware graph path for this board. */
			board_to_path(brd, path_buffer);

			/* Add the router */
			rv = hwgraph_path_add(hwgraph_root, path_buffer, &node_vertex);
			if (rv != GRAPH_SUCCESS)
				panic("Router vertex creation "
						  "failed.  Path == %s",
					path_buffer);

			HWGRAPH_DEBUG((__FILE__, __FUNCTION__, __LINE__, node_vertex, NULL, "Created router path.\n"));

		/* Find the rest of the routers stored on this node. */
		} while ( (brd = find_lboard_class(KLCF_NEXT(brd),
			 KLTYPE_ROUTER)) );
	}

}

/* ARGSUSED */
void
klhwg_connect_one_router(vertex_hdl_t hwgraph_root, lboard_t *brd,
			 cnodeid_t cnode, nasid_t nasid)
{
	klrou_t *router;
	char path_buffer[50];
	char dest_path[50];
	vertex_hdl_t router_hndl;
	vertex_hdl_t dest_hndl;
	int rc;
	int port;
	lboard_t *dest_brd;

	/* Don't add duplicate boards. */
	if (brd->brd_flags & DUPLICATE_BOARD) {
		return;
	}

	/* Generate a hardware graph path for this board. */
	board_to_path(brd, path_buffer);

	rc = hwgraph_traverse(hwgraph_root, path_buffer, &router_hndl);

	if (rc != GRAPH_SUCCESS && is_specified(arg_maxnodes))
			return;

	if (rc != GRAPH_SUCCESS)
		printk(KERN_WARNING  "Can't find router: %s", path_buffer);

	/* We don't know what to do with multiple router components */
	if (brd->brd_numcompts != 1) {
		panic("klhwg_connect_one_router: %d cmpts on router\n",
			brd->brd_numcompts);
		return;
	}


	/* Convert component 0 to klrou_t ptr */
	router = (klrou_t *)NODE_OFFSET_TO_K0(NASID_GET(brd),
					      brd->brd_compts[0]);

	for (port = 1; port <= MAX_ROUTER_PORTS; port++) {
		/* See if the port's active */
		if (router->rou_port[port].port_nasid == INVALID_NASID) {
			GRPRINTF(("klhwg_connect_one_router: port %d inactive.\n",
				 port));
			continue;
		}
		if (is_specified(arg_maxnodes) && NASID_TO_COMPACT_NODEID(router->rou_port[port].port_nasid) 
		    == INVALID_CNODEID) {
			continue;
		}

		dest_brd = (lboard_t *)NODE_OFFSET_TO_K0(
				router->rou_port[port].port_nasid,
				router->rou_port[port].port_offset);

		/* Generate a hardware graph path for this board. */
		board_to_path(dest_brd, dest_path);

		rc = hwgraph_traverse(hwgraph_root, dest_path, &dest_hndl);

		if (rc != GRAPH_SUCCESS) {
			if (is_specified(arg_maxnodes) && KL_CONFIG_DUPLICATE_BOARD(dest_brd))
				continue;
			panic("Can't find router: %s", dest_path);
		}

		sprintf(dest_path, "%d", port);

		rc = hwgraph_edge_add(router_hndl, dest_hndl, dest_path);
		if (rc == GRAPH_DUP) {
			GRPRINTF(("Skipping port %d. nasid %d %s/%s\n",
				  port, router->rou_port[port].port_nasid,
				  path_buffer, dest_path));
			continue;
		}

		if (rc != GRAPH_SUCCESS && !is_specified(arg_maxnodes))
			panic("Can't create edge: %s/%s to vertex 0x%p error 0x%x\n",
				path_buffer, dest_path, (void *)dest_hndl, rc);

		HWGRAPH_DEBUG((__FILE__, __FUNCTION__, __LINE__, router_hndl, dest_hndl, "Created edge %s from vhdl1 to vhdl2.\n", dest_path));
		
	}
}


void
klhwg_connect_routers(vertex_hdl_t hwgraph_root)
{
	nasid_t nasid;
	cnodeid_t cnode;
	lboard_t *brd;

	for (cnode = 0; cnode < numnodes; cnode++) {
		nasid = COMPACT_TO_NASID_NODEID(cnode);

		GRPRINTF(("klhwg_connect_routers: Connecting routers on cnode %d\n",
			cnode));

		brd = find_lboard_class((lboard_t *)KL_CONFIG_INFO(nasid),
				KLTYPE_ROUTER);

		if (!brd)
			continue;

		do {

			nasid = COMPACT_TO_NASID_NODEID(cnode);

			klhwg_connect_one_router(hwgraph_root, brd,
						 cnode, nasid);

		/* Find the rest of the routers stored on this node. */
		} while ( (brd = find_lboard_class(KLCF_NEXT(brd), KLTYPE_ROUTER)) );
	}
}



void
klhwg_connect_hubs(vertex_hdl_t hwgraph_root)
{
	nasid_t nasid;
	cnodeid_t cnode;
	lboard_t *brd;
	klhub_t *hub;
	lboard_t *dest_brd;
	vertex_hdl_t hub_hndl;
	vertex_hdl_t dest_hndl;
	char path_buffer[50];
	char dest_path[50];
	graph_error_t rc;
	int port;

	for (cnode = 0; cnode < numionodes; cnode++) {
		nasid = COMPACT_TO_NASID_NODEID(cnode);

		if (!(nasid & 1)) {
			brd = find_lboard((lboard_t *)KL_CONFIG_INFO(nasid), KLTYPE_SNIA);
			ASSERT(brd);
		} else {
			brd = find_lboard((lboard_t *)KL_CONFIG_INFO(nasid), KLTYPE_TIO);
			ASSERT(brd);
		}

		hub = (klhub_t *)find_first_component(brd, KLSTRUCT_HUB);
		ASSERT(hub);

		for (port = 1; port <= MAX_NI_PORTS; port++) {
			/* See if the port's active */
			if (hub->hub_port[port].port_nasid == INVALID_NASID) {
				GRPRINTF(("klhwg_connect_hubs: port inactive.\n"));
				continue;
			}

			if (is_specified(arg_maxnodes) && NASID_TO_COMPACT_NODEID(hub->hub_port[port].port_nasid) == INVALID_CNODEID)
				continue;

			/* Generate a hardware graph path for this board. */
			board_to_path(brd, path_buffer);
			rc = hwgraph_traverse(hwgraph_root, path_buffer, &hub_hndl);
			if (rc != GRAPH_SUCCESS)
				printk(KERN_WARNING  "Can't find hub: %s", path_buffer);

			dest_brd = (lboard_t *)NODE_OFFSET_TO_K0(
					hub->hub_port[port].port_nasid,
					hub->hub_port[port].port_offset);

			/* Generate a hardware graph path for this board. */
			board_to_path(dest_brd, dest_path);

			rc = hwgraph_traverse(hwgraph_root, dest_path, &dest_hndl);

			if (rc != GRAPH_SUCCESS) {
				if (is_specified(arg_maxnodes) && KL_CONFIG_DUPLICATE_BOARD(dest_brd))
					continue;
				panic("Can't find board: %s", dest_path);
			} else {
				char buf[1024];
		

				GRPRINTF(("klhwg_connect_hubs: Link from %s to %s.\n",
			  	path_buffer, dest_path));

				rc = hwgraph_path_add(hub_hndl, EDGE_LBL_INTERCONNECT, &hub_hndl);

				HWGRAPH_DEBUG((__FILE__, __FUNCTION__, __LINE__, hub_hndl, NULL, "Created link path.\n"));

				sprintf(buf,"%s/%s",path_buffer,EDGE_LBL_INTERCONNECT);
				rc = hwgraph_traverse(hwgraph_root, buf, &hub_hndl);
				sprintf(buf,"%d",port);
				rc = hwgraph_edge_add(hub_hndl, dest_hndl, buf);

				HWGRAPH_DEBUG((__FILE__, __FUNCTION__, __LINE__, hub_hndl, dest_hndl, "Created edge %s from vhdl1 to vhdl2.\n", buf));

				if (rc != GRAPH_SUCCESS)
					panic("Can't create edge: %s/%s to vertex 0x%p, error 0x%x\n",
					path_buffer, dest_path, (void *)dest_hndl, rc);

			}
		}
	}
}

/* Store the pci/vme disabled board information as extended administrative
 * hints which can later be used by the drivers using the device/driver
 * admin interface. 
 */
void
klhwg_device_disable_hints_add(void)
{
	cnodeid_t	cnode; 		/* node we are looking at */
	nasid_t		nasid;		/* nasid of the node */
	lboard_t	*board;		/* board we are looking at */
	int		comp_index;	/* component index */
	klinfo_t	*component;	/* component in the board we are
					 * looking at 
					 */
	char		device_name[MAXDEVNAME];
	
	for(cnode = 0; cnode < numnodes; cnode++) {
		nasid = COMPACT_TO_NASID_NODEID(cnode);
		board = (lboard_t *)KL_CONFIG_INFO(nasid);
		/* Check out all the board info stored  on a node */
		while(board) {
			/* No need to look at duplicate boards or non-io 
			 * boards
			 */
			if (KL_CONFIG_DUPLICATE_BOARD(board) ||
			    KLCLASS(board->brd_type) != KLCLASS_IO) {
				board = KLCF_NEXT(board);
				continue;
			}
			/* Check out all the components of a board */
			for (comp_index = 0; 
			     comp_index < KLCF_NUM_COMPS(board);
			     comp_index++) {
				component = KLCF_COMP(board,comp_index);
				/* If the component is enabled move on to
				 * the next component
				 */
				if (KLCONFIG_INFO_ENABLED(component))
					continue;
				/* NOTE : Since the prom only supports
				 * the disabling of pci devices the following
				 * piece of code makes sense. 
				 * Make sure that this assumption is valid
				 */
				/* This component is disabled. Store this
				 * hint in the extended device admin table
				 */
				/* Get the canonical name of the pci device */
				device_component_canonical_name_get(board,
							    component,
							    device_name);
#ifdef DEBUG
				printf("%s DISABLED\n",device_name);
#endif				
			}
			/* go to the next board info stored on this 
			 * node 
			 */
			board = KLCF_NEXT(board);
		}
	}
}

void
klhwg_add_all_modules(vertex_hdl_t hwgraph_root)
{
	cmoduleid_t	cm;
	char		name[128];
	vertex_hdl_t	vhdl;
	vertex_hdl_t  module_vhdl;
	int		rc;
	char		buffer[16];

	/* Add devices under each module */

	for (cm = 0; cm < nummodules; cm++) {
		/* Use module as module vertex fastinfo */

		memset(buffer, 0, 16);
		format_module_id(buffer, modules[cm]->id, MODULE_FORMAT_BRIEF);
		sprintf(name, EDGE_LBL_MODULE "/%s", buffer);

		rc = hwgraph_path_add(hwgraph_root, name, &module_vhdl);
		ASSERT(rc == GRAPH_SUCCESS);
		rc = rc; /* Shut the compiler up */
		HWGRAPH_DEBUG((__FILE__, __FUNCTION__, __LINE__, module_vhdl, NULL, "Created module path.\n"));

		hwgraph_fastinfo_set(module_vhdl, (arbitrary_info_t) modules[cm]);

		/* Add system controller */
		sprintf(name,
			EDGE_LBL_MODULE "/%s/" EDGE_LBL_L1,
			buffer);

		rc = hwgraph_path_add(hwgraph_root, name, &vhdl);
		ASSERT_ALWAYS(rc == GRAPH_SUCCESS);
		rc = rc; /* Shut the compiler up */
		HWGRAPH_DEBUG((__FILE__, __FUNCTION__, __LINE__, vhdl, NULL, "Created L1 path.\n"));

		hwgraph_info_add_LBL(vhdl,
				     INFO_LBL_ELSC,
				     (arbitrary_info_t) (int64_t) 1);

	}
}

void
klhwg_add_all_nodes(vertex_hdl_t hwgraph_root)
{
	cnodeid_t	cnode;

	for (cnode = 0; cnode < numnodes; cnode++) {
		klhwg_add_node(hwgraph_root, cnode);
	}

	for (cnode = numnodes; cnode < numionodes; cnode++) {
		klhwg_add_tionode(hwgraph_root, cnode);
	}

	for (cnode = 0; cnode < numnodes; cnode++) {
		klhwg_add_xbow(cnode, cnodeid_to_nasid(cnode));
	}

	for (cnode = numnodes; cnode < numionodes; cnode++) {
		klhwg_add_coretalk(cnode, cnodeid_to_nasid(cnode));
	}

	/*
	 * As for router hardware inventory information, we set this
	 * up in router.c. 
	 */
	
	klhwg_add_all_routers(hwgraph_root);
	klhwg_connect_routers(hwgraph_root);
	klhwg_connect_hubs(hwgraph_root);

	/* Go through the entire system's klconfig
	 * to figure out which pci components have been disabled
	 */
	klhwg_device_disable_hints_add();

}
