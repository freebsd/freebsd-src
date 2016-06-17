/*********************************************************************
 *                
 * Filename:      irlan_filter.c
 * Version:       
 * Description:   
 * Status:        Experimental.
 * Author:        Dag Brattli <dagb@cs.uit.no>
 * Created at:    Fri Jan 29 11:16:38 1999
 * Modified at:   Sat Oct 30 12:58:45 1999
 * Modified by:   Dag Brattli <dagb@cs.uit.no>
 * 
 *     Copyright (c) 1998-1999 Dag Brattli, All Rights Reserved.
 *      
 *     This program is free software; you can redistribute it and/or 
 *     modify it under the terms of the GNU General Public License as 
 *     published by the Free Software Foundation; either version 2 of 
 *     the License, or (at your option) any later version.
 *  
 *     Neither Dag Brattli nor University of Tromsø admit liability nor
 *     provide warranty for any of this software. This material is 
 *     provided "AS-IS" and at no charge.
 *     
 ********************************************************************/

#include <linux/skbuff.h>
#include <linux/random.h>

#include <net/irda/irlan_common.h>

/*
 * Function handle_filter_request (self, skb)
 *
 *    Handle filter request from client peer device
 *
 */
void handle_filter_request(struct irlan_cb *self, struct sk_buff *skb)
{
	ASSERT(self != NULL, return;);
	ASSERT(self->magic == IRLAN_MAGIC, return;);

	if ((self->provider.filter_type == IRLAN_DIRECTED) && 
	    (self->provider.filter_operation == DYNAMIC))
	{
		IRDA_DEBUG(0, "Giving peer a dynamic Ethernet address\n");
		self->provider.mac_address[0] = 0x40;
		self->provider.mac_address[1] = 0x00;
		self->provider.mac_address[2] = 0x00;
		self->provider.mac_address[3] = 0x00;
		
		/* Use arbitration value to generate MAC address */
		if (self->provider.access_type == ACCESS_PEER) {
			self->provider.mac_address[4] = 
				self->provider.send_arb_val & 0xff;
			self->provider.mac_address[5] = 
				(self->provider.send_arb_val >> 8) & 0xff;;
		} else {
			/* Just generate something for now */
			get_random_bytes(self->provider.mac_address+4, 1);
			get_random_bytes(self->provider.mac_address+5, 1);
		}

		skb->data[0] = 0x00; /* Success */
		skb->data[1] = 0x03;
		irlan_insert_string_param(skb, "FILTER_MODE", "NONE");
		irlan_insert_short_param(skb, "MAX_ENTRY", 0x0001);
		irlan_insert_array_param(skb, "FILTER_ENTRY", 
					 self->provider.mac_address, 6);
		return;
	}
	
	if ((self->provider.filter_type == IRLAN_DIRECTED) && 
	    (self->provider.filter_mode == FILTER))
	{
		IRDA_DEBUG(0, "Directed filter on\n");
		skb->data[0] = 0x00; /* Success */
		skb->data[1] = 0x00;
		return;
	}
	if ((self->provider.filter_type == IRLAN_DIRECTED) && 
	    (self->provider.filter_mode == NONE))
	{
		IRDA_DEBUG(0, "Directed filter off\n");
		skb->data[0] = 0x00; /* Success */
		skb->data[1] = 0x00;
		return;
	}

	if ((self->provider.filter_type == IRLAN_BROADCAST) && 
	    (self->provider.filter_mode == FILTER))
	{
		IRDA_DEBUG(0, "Broadcast filter on\n");
		skb->data[0] = 0x00; /* Success */
		skb->data[1] = 0x00;
		return;
	}
	if ((self->provider.filter_type == IRLAN_BROADCAST) && 
	    (self->provider.filter_mode == NONE))
	{
		IRDA_DEBUG(0, "Broadcast filter off\n");
		skb->data[0] = 0x00; /* Success */
		skb->data[1] = 0x00;
		return;
	}
	if ((self->provider.filter_type == IRLAN_MULTICAST) && 
	    (self->provider.filter_mode == FILTER))
	{
		IRDA_DEBUG(0, "Multicast filter on\n");
		skb->data[0] = 0x00; /* Success */
		skb->data[1] = 0x00;
		return;
	}
	if ((self->provider.filter_type == IRLAN_MULTICAST) && 
	    (self->provider.filter_mode == NONE))
	{
		IRDA_DEBUG(0, "Multicast filter off\n");
		skb->data[0] = 0x00; /* Success */
		skb->data[1] = 0x00;
		return;
	}
	if ((self->provider.filter_type == IRLAN_MULTICAST) && 
	    (self->provider.filter_operation == GET))
	{
		IRDA_DEBUG(0, "Multicast filter get\n");
		skb->data[0] = 0x00; /* Success? */
		skb->data[1] = 0x02;
		irlan_insert_string_param(skb, "FILTER_MODE", "NONE");
		irlan_insert_short_param(skb, "MAX_ENTRY", 16);
		return;
	}
	skb->data[0] = 0x00; /* Command not supported */
	skb->data[1] = 0x00;

	IRDA_DEBUG(0, "Not implemented!\n");
}

/*
 * Function check_request_param (self, param, value)
 *
 *    Check parameters in request from peer device
 *
 */
void irlan_check_command_param(struct irlan_cb *self, char *param, char *value)
{
	__u8 *bytes;

	IRDA_DEBUG(4, "%s()\n", __FUNCTION__);

	bytes = value;

	ASSERT(self != NULL, return;);
	ASSERT(self->magic == IRLAN_MAGIC, return;);

	IRDA_DEBUG(4, "%s, %s\n", param, value);

	/*
	 *  This is experimental!! DB.
	 */
	 if (strcmp(param, "MODE") == 0) {
		IRDA_DEBUG(0, "%s()\n", __FUNCTION__);
		self->use_udata = TRUE;
		return;
	}

	/*
	 *  FILTER_TYPE
	 */
	if (strcmp(param, "FILTER_TYPE") == 0) {
		if (strcmp(value, "DIRECTED") == 0) {
			self->provider.filter_type = IRLAN_DIRECTED;
			return;
		}
		if (strcmp(value, "MULTICAST") == 0) {
			self->provider.filter_type = IRLAN_MULTICAST;
			return;
		}
		if (strcmp(value, "BROADCAST") == 0) {
			self->provider.filter_type = IRLAN_BROADCAST;
			return;
		}
	}
	/*
	 *  FILTER_MODE
	 */
	if (strcmp(param, "FILTER_MODE") == 0) {
		if (strcmp(value, "ALL") == 0) {
			self->provider.filter_mode = ALL;
			return;
		}
		if (strcmp(value, "FILTER") == 0) {
			self->provider.filter_mode = FILTER;
			return;
		}
		if (strcmp(value, "NONE") == 0) {
			self->provider.filter_mode = FILTER;
			return;
		}
	}
	/*
	 *  FILTER_OPERATION
	 */
	if (strcmp(param, "FILTER_OPERATION") == 0) {
		if (strcmp(value, "DYNAMIC") == 0) {
			self->provider.filter_operation = DYNAMIC;
			return;
		}
		if (strcmp(value, "GET") == 0) {
			self->provider.filter_operation = GET;
			return;
		}
	}
}

/*
 * Function irlan_print_filter (filter_type, buf)
 *
 *    Print status of filter. Used by /proc file system
 *
 */
int irlan_print_filter(int filter_type, char *buf)
{
	int len = 0;

	if (filter_type & IRLAN_DIRECTED)
		len += sprintf(buf+len, "%s", "DIRECTED ");
	if (filter_type & IRLAN_FUNCTIONAL)
		len += sprintf(buf+len, "%s", "FUNCTIONAL ");
	if (filter_type & IRLAN_GROUP)
		len += sprintf(buf+len, "%s", "GROUP ");
	if (filter_type & IRLAN_MAC_FRAME)
		len += sprintf(buf+len, "%s", "MAC_FRAME ");
	if (filter_type & IRLAN_MULTICAST)
		len += sprintf(buf+len, "%s", "MULTICAST ");
	if (filter_type & IRLAN_BROADCAST)
		len += sprintf(buf+len, "%s", "BROADCAST ");
	if (filter_type & IRLAN_IPX_SOCKET)
		len += sprintf(buf+len, "%s", "IPX_SOCKET");

	len += sprintf(buf+len, "\n");

	return len;
}
