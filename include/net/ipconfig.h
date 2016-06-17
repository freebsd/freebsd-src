/*
 *  $Id: ipconfig.h,v 1.4 2001/04/30 04:51:46 davem Exp $
 *
 *  Copyright (C) 1997 Martin Mares
 *
 *  Automatic IP Layer Configuration
 */

/* The following are initdata: */

extern int ic_enable;		/* Enable or disable the whole shebang */

extern int ic_proto_enabled;	/* Protocols enabled (see IC_xxx) */
extern int ic_host_name_set;	/* Host name set by ipconfig? */
extern int ic_set_manually;	/* IPconfig parameters set manually */

extern u32 ic_myaddr;		/* My IP address */
extern u32 ic_netmask;		/* Netmask for local subnet */
extern u32 ic_gateway;		/* Gateway IP address */

extern u32 ic_servaddr;		/* Boot server IP address */

extern u32 root_server_addr;	/* Address of NFS server */
extern u8 root_server_path[];	/* Path to mount as root */



/* The following are persistent (not initdata): */

extern int ic_proto_used;	/* Protocol used, if any */
extern u32 ic_nameserver;	/* DNS server IP address */
extern u8 ic_domain[];		/* DNS (not NIS) domain name */

/* bits in ic_proto_{enabled,used} */
#define IC_PROTO	0xFF	/* Protocols mask: */
#define IC_BOOTP	0x01	/*   BOOTP (or DHCP, see below) */
#define IC_RARP		0x02	/*   RARP */
#define IC_USE_DHCP    0x100	/* If on, use DHCP instead of BOOTP */
