/* $Id$
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1992-1997,2000-2003 Silicon Graphics, Inc. All rights reserved.
 */

#include <linux/config.h>
#include <linux/types.h>
#include <asm/sn/sgi.h>
#include <asm/sn/sn_sal.h>
#include <asm/sn/io.h>
#include <asm/sn/sn_cpuid.h>
#include <asm/sn/klconfig.h>
#include <asm/sn/module.h>
#include <asm/sn/sn_private.h>

cpuid_t         master_procid;
char arg_maxnodes[4];

/*
 * Return non-zero if the given variable was specified
 */
int
is_specified(char *s)
{
        return (strlen(s) != 0);
}

/*
 * Routines provided by ml/SN/promif.c.
 */
static __psunsigned_t master_bridge_base;
nasid_t console_nasid = (nasid_t)-1;
char master_baseio_wid;
static char console_wid;
static char console_pcislot;

void
set_master_bridge_base(void)
{
        console_wid = WIDGETID_GET(KL_CONFIG_CH_CONS_INFO(master_nasid)->memory_base);
        console_pcislot = KL_CONFIG_CH_CONS_INFO(master_nasid)->npci;
        master_bridge_base = (__psunsigned_t)NODE_SWIN_BASE(console_nasid, console_wid);
	// FIXME("WARNING: set_master_bridge_base: NON NASID 0 DOES NOT WORK\n");
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
	extern nasid_t master_baseio_nasid;

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
