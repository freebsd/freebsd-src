/*
 * Copyright (c) 1995, 1996
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef lint
static const char rcsid[] =
    "@(#) $Header: print-pim.c,v 1.7 96/09/26 23:36:48 leres Exp $ (LBL)";
#endif

#include <sys/param.h>
#include <sys/time.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <netinet/tcp.h>
#include <netinet/tcpip.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "interface.h"
#include "addrtoname.h"

void
pim_print(register const u_char *bp, register u_int len)
{
    register const u_char *ep;
    register u_char type;

    ep = (const u_char *)snapend;
    if (bp >= ep)
	return;

    type = bp[1];

    switch (type) {
    case 0:
	(void)printf(" Query");
	break;

    case 1:
	(void)printf(" Register");
	break;

    case 2:
	(void)printf(" Register-Stop");
	break;

    case 3:
	(void)printf(" Join/Prune");
	break;

    case 4:
	(void)printf(" RP-reachable");
	break;

    case 5:
	(void)printf(" Assert");
	break;

    case 6:
	(void)printf(" Graft");
	break;

    case 7:
	(void)printf(" Graft-ACK");
	break;

    case 8:
	(void)printf(" Mode");
	break;

    default:
	(void)printf(" [type %d]", type);
	break;
    }
}
