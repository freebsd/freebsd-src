/*
 * Copyright (c) 1990,1991 Regents of The University of Michigan.
 * All Rights Reserved.
 */

#include <net/if_llc.h>

#define llc_org_code llc_un.type_snap.org_code
#define llc_ether_type llc_un.type_snap.ether_type

#define SIOCPHASE1	_IOW('i', 100, struct ifreq)	/* AppleTalk phase 1 */
#define SIOCPHASE2	_IOW('i', 101, struct ifreq)	/* AppleTalk phase 2 */
