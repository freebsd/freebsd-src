/* $Id$
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1992 - 1997, 2000-2003 Silicon Graphics, Inc. All rights reserved.
 */

#include <linux/types.h>
#include <linux/config.h>
#include <linux/slab.h>
#include <asm/sn/sgi.h>
#include <asm/sn/io.h>
#include <asm/sn/sn_cpuid.h>
#include <asm/sn/klconfig.h>
#include <asm/sn/sn_private.h>
#include <asm/sn/pci/pciba.h>
#include <linux/smp.h>
#include <asm/sn/simulator.h>

extern void init_all_devices(void);
extern void klhwg_add_all_modules(vertex_hdl_t);
extern void klhwg_add_all_nodes(vertex_hdl_t);

extern int init_hcl(void);
extern vertex_hdl_t hwgraph_root;
extern void io_module_init(void);
extern int pci_bus_to_hcl_cvlink(void);

char arg_maxnodes[4];
char master_baseio_wid;
nasid_t master_baseio_nasid;
nasid_t master_nasid = INVALID_NASID;           /* This is the partition master nasid */
nasid_t console_nasid = (nasid_t)-1;

/*
 * Return non-zero if the given variable was specified
 */
int
is_specified(char *s)
{
	return (strlen(s) != 0);
}

int
check_nasid_equiv(nasid_t nasida, nasid_t nasidb)
{
	if ((nasida == nasidb) || (nasida == NODEPDA(NASID_TO_COMPACT_NODEID(nasidb))->xbow_peer))
		return 1;
	else
		return 0;
}

int
is_master_baseio_nasid_widget(nasid_t test_nasid, xwidgetnum_t test_wid)
{

	/*
	 * If the widget numbers are different, we're not the master.
	 */
	if (test_wid != (xwidgetnum_t)master_baseio_wid) {
		return 0;
	}

	/*
	 * If the NASIDs are the same or equivalent, we're the master.
	 */
	if (check_nasid_equiv(test_nasid, master_baseio_nasid)) {
		return 1;
	} else {
		return 0;
	}
}

/*
 * This routine is responsible for the setup of all the IRIX hwgraph style
 * stuff that's been pulled into linux.  It's called by sn_pci_find_bios which
 * is called just before the generic Linux PCI layer does its probing (by 
 * platform_pci_fixup aka sn_pci_fixup).
 *
 * It is very IMPORTANT that this call is only made by the Master CPU!
 *
 */

void
sgi_master_io_infr_init(void)
{
	cnodeid_t cnode;

	init_hcl(); /* Sets up the hwgraph compatibility layer */

        /*
         * Initialize platform-dependent vertices in the hwgraph:
         *      module
         *      node
         *      cpu
         *      memory
         *      slot
         *      hub
         *      router
         *      xbow
         */

        io_module_init(); /* Use to be called module_init() .. */
        klhwg_add_all_modules(hwgraph_root);
        klhwg_add_all_nodes(hwgraph_root);

	for (cnode = 0; cnode < numnodes; cnode++) {
		extern void per_hub_init(cnodeid_t);
		per_hub_init(cnode);
	}

	/* We can do headless hub cnodes here .. */

	/* Initialize ICE for TIO Nodes. */
	for (cnode = numnodes; cnode < numionodes; cnode++) {
		extern void per_ice_init(cnodeid_t);
		per_ice_init(cnode);
	}

	/*
	 *
	 * Our IO Infrastructure drivers are in place .. 
	 * Initialize the whole IO Infrastructure .. xwidget/device probes.
	 *
	 */
	init_all_devices();
	pci_bus_to_hcl_cvlink();

#ifdef  CONFIG_KDB
        {
                extern void kdba_io_init(void);
                kdba_io_init();
        }
#endif

}
