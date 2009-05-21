/*
 * Copyright (c) 2007 - Andrey "nording" Chernyak <andrew@nording.ru>
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
 *
 * Format and print Realtek Remote Control Protocol (RRCP)
 * and Realtek Echo Protocol (RRCP-REP) packets.
 */

#ifndef lint
static const char rcsid[] _U_ =
    "@(#) $Header: /tcpdump/master/tcpdump/print-rrcp.c,v 1.1.2.2 2008-04-11 17:00:00 gianluca Exp $";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <tcpdump-stdinc.h>

#include <stdio.h>
#include <string.h>

#include "netdissect.h"
#include "addrtoname.h"
#include "extract.h"
#include "ether.h"

#ifndef ETH_ALEN 
#define ETH_ALEN 6
#endif

struct rrcp_packet_t
{
  u_int16_t rrcp_ethertype;		/* 0x8899 */
  u_int8_t  rrcp_proto;			/* must be 0x01         */
  u_int8_t  rrcp_opcode:7;               /* 0x00 = hello, 0x01 = get, 0x02 = set */
  u_int8_t  rrcp_isreply:1;              /* 0 = request to switch, 1 = reply from switch */
  u_int16_t rrcp_authkey;		/* 0x2379 by default */
  u_int16_t rrcp_reg_addr;		/* register address */
  u_int32_t rrcp_reg_data;		/* register data */
  u_int32_t cookie1;
  u_int32_t cookie2;
};

struct rrcp_helloreply_packet_t
{
  u_int16_t rrcp_ethertype;		/* 0x8899 */
  u_int8_t  rrcp_proto;			/* must be 0x01         */
  u_int8_t  rrcp_opcode:7;               /* 0x00 = hello, 0x01 = get, 0x02 = set */
  u_int8_t  rrcp_isreply:1;              /* 0 = request to switch, 1 = reply from switch */
  u_int16_t rrcp_authkey;		/* 0x2379 by default */
  u_int8_t  rrcp_downlink_port;		/*  */
  u_int8_t  rrcp_uplink_port;		/*  */
  u_int8_t  rrcp_uplink_mac[ETH_ALEN];   /*  */
  u_int16_t rrcp_chip_id;		/*  */
  u_int32_t rrcp_vendor_id;		/*  */
};


/*
 * Print RRCP requests
 */
void
rrcp_print(netdissect_options *ndo,
	  register const u_char *cp,
	  u_int length _U_)
{
	const struct rrcp_packet_t *rrcp;
	const struct rrcp_helloreply_packet_t *rrcp_hello;
	register const struct ether_header *ep;
	char proto_str[16];
	char opcode_str[32];

	ep = (const struct ether_header *)cp;
	rrcp = (const struct rrcp_packet_t *)(cp+12);
	rrcp_hello = (const struct rrcp_helloreply_packet_t *)(cp+12);

	if (rrcp->rrcp_proto==1){
	    strcpy(proto_str,"RRCP");
	}else if ( rrcp->rrcp_proto==2 ){
	    strcpy(proto_str,"RRCP-REP");
	}else{
	    sprintf(proto_str,"RRCP-0x%02d",rrcp->rrcp_proto);
	}
	if (rrcp->rrcp_opcode==0){
	    strcpy(opcode_str,"hello");
	}else if ( rrcp->rrcp_opcode==1 ){
	    strcpy(opcode_str,"get");
	}else if ( rrcp->rrcp_opcode==2 ){
	    strcpy(opcode_str,"set");
	}else{
	    sprintf(opcode_str,"unknown opcode (0x%02d)",rrcp->rrcp_opcode);
	}
        ND_PRINT((ndo, "%s > %s, %s %s",
		etheraddr_string(ESRC(ep)),
		etheraddr_string(EDST(ep)),
		proto_str, rrcp->rrcp_isreply ? "reply" : "query"));
	if (rrcp->rrcp_proto==1){
    	    ND_PRINT((ndo, ": %s", opcode_str));
	}
	if (rrcp->rrcp_opcode==1 || rrcp->rrcp_opcode==2){
    	    ND_PRINT((ndo, " addr=0x%04x, data=0x%04x",
		     rrcp->rrcp_reg_addr, rrcp->rrcp_reg_data, rrcp->rrcp_authkey));
	}
	if (rrcp->rrcp_proto==1){
    	    ND_PRINT((ndo, ", auth=0x%04x",
		  ntohs(rrcp->rrcp_authkey)));
	}
	if (rrcp->rrcp_proto==1 && rrcp->rrcp_opcode==0 && rrcp->rrcp_isreply){
    	    ND_PRINT((ndo, " downlink_port=%d, uplink_port=%d, uplink_mac=%s, vendor_id=%08x ,chip_id=%04x ",
		     rrcp_hello->rrcp_downlink_port,
		     rrcp_hello->rrcp_uplink_port,
		     etheraddr_string(rrcp_hello->rrcp_uplink_mac),
		     rrcp_hello->rrcp_vendor_id,
		     rrcp_hello->rrcp_chip_id));
	}else if (rrcp->rrcp_opcode==1 || rrcp->rrcp_opcode==2 || rrcp->rrcp_proto==2){
    	ND_PRINT((ndo, ", cookie=0x%08x%08x ",
		    rrcp->cookie2, rrcp->cookie1));
	}
        if (!ndo->ndo_vflag)
            return;
}
