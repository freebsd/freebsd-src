/*
 * Copyright (c) 2006 QLogic, Inc.  All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef VNIC_UTIL_H_INCLUDED
#define VNIC_UTIL_H_INCLUDED

#define MODULE_NAME "QLGC_VNIC"

#define VNIC_MAJORVERSION	1
#define VNIC_MINORVERSION	1

#define ALIGN_DOWN(x, a)	((x)&(~((a)-1)))

extern u32 vnic_debug;

enum {
	DEBUG_IB_INFO			= 0x00000001,
	DEBUG_IB_FUNCTION		= 0x00000002,
	DEBUG_IB_FSTATUS		= 0x00000004,
	DEBUG_IB_ASSERTS		= 0x00000008,
	DEBUG_CONTROL_INFO		= 0x00000010,
	DEBUG_CONTROL_FUNCTION	= 0x00000020,
	DEBUG_CONTROL_PACKET	= 0x00000040,
	DEBUG_CONFIG_INFO		= 0x00000100,
	DEBUG_DATA_INFO 		= 0x00001000,
	DEBUG_DATA_FUNCTION		= 0x00002000,
	DEBUG_NETPATH_INFO		= 0x00010000,
	DEBUG_VIPORT_INFO		= 0x00100000,
	DEBUG_VIPORT_FUNCTION	= 0x00200000,
	DEBUG_LINK_STATE		= 0x00400000,
	DEBUG_VNIC_INFO 		= 0x01000000,
	DEBUG_VNIC_FUNCTION		= 0x02000000,
	DEBUG_MCAST_INFO		= 0x04000000,
	DEBUG_MCAST_FUNCTION	= 0x08000000,
	DEBUG_SYS_INFO			= 0x10000000,
	DEBUG_SYS_VERBOSE		= 0x40000000
};

#define PRINT(level, x, fmt, arg...)					\
	printk(level "%s: " fmt, MODULE_NAME, ##arg)

#define PRINT_CONDITIONAL(level, x, condition, fmt, arg...)		\
	do {								\
		 if (condition)						\
			printk(level "%s: %s: " fmt,			\
			       MODULE_NAME, x, ##arg);			\
	} while (0)

#define IB_PRINT(fmt, arg...)			\
	PRINT(KERN_INFO, "IB", fmt, ##arg)
#define IB_ERROR(fmt, arg...)			\
	PRINT(KERN_ERR, "IB", fmt, ##arg)

#define IB_FUNCTION(fmt, arg...) 				\
	PRINT_CONDITIONAL(KERN_INFO, 				\
			  "IB", 				\
			  (vnic_debug & DEBUG_IB_FUNCTION), 	\
			  fmt, ##arg)

#define IB_INFO(fmt, arg...)					\
	PRINT_CONDITIONAL(KERN_INFO,				\
			  "IB",					\
			  (vnic_debug & DEBUG_IB_INFO),		\
			  fmt, ##arg)

#define IB_ASSERT(x)							\
	do {								\
		 if ((vnic_debug & DEBUG_IB_ASSERTS) && !(x))		\
			panic("%s assertion failed, file:  %s,"		\
				" line %d: ",				\
				MODULE_NAME, __FILE__, __LINE__)	\
	} while (0)

#define CONTROL_PRINT(fmt, arg...)			\
	PRINT(KERN_INFO, "CONTROL", fmt, ##arg)
#define CONTROL_ERROR(fmt, arg...)			\
	PRINT(KERN_ERR, "CONTROL", fmt, ##arg)

#define CONTROL_INFO(fmt, arg...)					\
	PRINT_CONDITIONAL(KERN_INFO,					\
			  "CONTROL",					\
			  (vnic_debug & DEBUG_CONTROL_INFO),		\
			  fmt, ##arg)

#define CONTROL_FUNCTION(fmt, arg...)					\
	PRINT_CONDITIONAL(KERN_INFO,					\
			"CONTROL",					\
			(vnic_debug & DEBUG_CONTROL_FUNCTION),		\
			fmt, ##arg)

#define CONTROL_PACKET(pkt)					\
	do {							\
		 if (vnic_debug & DEBUG_CONTROL_PACKET)		\
			control_log_control_packet(pkt);	\
	} while (0)

#define CONFIG_PRINT(fmt, arg...)		\
	PRINT(KERN_INFO, "CONFIG", fmt, ##arg)
#define CONFIG_ERROR(fmt, arg...)		\
	PRINT(KERN_ERR, "CONFIG", fmt, ##arg)

#define CONFIG_INFO(fmt, arg...)				\
	PRINT_CONDITIONAL(KERN_INFO,				\
			  "CONFIG",				\
			  (vnic_debug & DEBUG_CONFIG_INFO),	\
			  fmt, ##arg)

#define DATA_PRINT(fmt, arg...)			\
	PRINT(KERN_INFO, "DATA", fmt, ##arg)
#define DATA_ERROR(fmt, arg...)			\
	PRINT(KERN_ERR, "DATA", fmt, ##arg)

#define DATA_INFO(fmt, arg...)					\
	PRINT_CONDITIONAL(KERN_INFO,				\
			  "DATA",				\
			  (vnic_debug & DEBUG_DATA_INFO),	\
			  fmt, ##arg)

#define DATA_FUNCTION(fmt, arg...)				\
	PRINT_CONDITIONAL(KERN_INFO,				\
			  "DATA",				\
			  (vnic_debug & DEBUG_DATA_FUNCTION),	\
			  fmt, ##arg)


#define MCAST_PRINT(fmt, arg...)        \
    PRINT(KERN_INFO, "MCAST", fmt, ##arg)
#define MCAST_ERROR(fmt, arg...)        \
    PRINT(KERN_ERR, "MCAST", fmt, ##arg)

#define MCAST_INFO(fmt, arg...)   	              		\
	PRINT_CONDITIONAL(KERN_INFO,     			\
			"MCAST",   				\
			(vnic_debug & DEBUG_MCAST_INFO),	\
			fmt, ##arg)

#define MCAST_FUNCTION(fmt, arg...)				\
	PRINT_CONDITIONAL(KERN_INFO,				\
			"MCAST",				\
			(vnic_debug & DEBUG_MCAST_FUNCTION), 	\
			fmt, ##arg)

#define NETPATH_PRINT(fmt, arg...)		\
	PRINT(KERN_INFO, "NETPATH", fmt, ##arg)
#define NETPATH_ERROR(fmt, arg...)		\
	PRINT(KERN_ERR, "NETPATH", fmt, ##arg)

#define NETPATH_INFO(fmt, arg...)				\
	PRINT_CONDITIONAL(KERN_INFO,				\
			  "NETPATH",				\
			  (vnic_debug & DEBUG_NETPATH_INFO),	\
			  fmt, ##arg)

#define VIPORT_PRINT(fmt, arg...)		\
	PRINT(KERN_INFO, "VIPORT", fmt, ##arg)
#define VIPORT_ERROR(fmt, arg...)		\
	PRINT(KERN_ERR, "VIPORT", fmt, ##arg)

#define VIPORT_INFO(fmt, arg...) 				\
	PRINT_CONDITIONAL(KERN_INFO,				\
			  "VIPORT",				\
			  (vnic_debug & DEBUG_VIPORT_INFO),	\
			  fmt, ##arg)

#define VIPORT_FUNCTION(fmt, arg...)				\
	PRINT_CONDITIONAL(KERN_INFO,				\
			  "VIPORT",				\
			  (vnic_debug & DEBUG_VIPORT_FUNCTION),	\
			  fmt, ##arg)

#define LINK_STATE(fmt, arg...) 				\
	PRINT_CONDITIONAL(KERN_INFO,				\
			  "LINK",				\
			  (vnic_debug & DEBUG_LINK_STATE),	\
			  fmt, ##arg)

#define VNIC_PRINT(fmt, arg...)			\
	PRINT(KERN_INFO, "NIC", fmt, ##arg)
#define VNIC_ERROR(fmt, arg...)			\
	PRINT(KERN_ERR, "NIC", fmt, ##arg)
#define VNIC_INIT(fmt, arg...)			\
	PRINT(KERN_INFO, "NIC", fmt, ##arg)

#define VNIC_INFO(fmt, arg...)					\
	 PRINT_CONDITIONAL(KERN_INFO,				\
			   "NIC",				\
			   (vnic_debug & DEBUG_VNIC_INFO),	\
			   fmt, ##arg)

#define VNIC_FUNCTION(fmt, arg...)				\
	 PRINT_CONDITIONAL(KERN_INFO,				\
			   "NIC",				\
			   (vnic_debug & DEBUG_VNIC_FUNCTION),	\
			   fmt, ##arg)

#define SYS_PRINT(fmt, arg...)			\
	PRINT(KERN_INFO, "SYS", fmt, ##arg)
#define SYS_ERROR(fmt, arg...)			\
	PRINT(KERN_ERR, "SYS", fmt, ##arg)

#define SYS_INFO(fmt, arg...)					\
	 PRINT_CONDITIONAL(KERN_INFO,				\
			   "SYS",				\
			   (vnic_debug & DEBUG_SYS_INFO),	\
			   fmt, ##arg)

#endif	/* VNIC_UTIL_H_INCLUDED */
