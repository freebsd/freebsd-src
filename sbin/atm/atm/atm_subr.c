/*
 *
 * ===================================
 * HARP  |  Host ATM Research Platform
 * ===================================
 *
 *
 * This Host ATM Research Platform ("HARP") file (the "Software") is
 * made available by Network Computing Services, Inc. ("NetworkCS")
 * "AS IS".  NetworkCS does not provide maintenance, improvements or
 * support of any kind.
 *
 * NETWORKCS MAKES NO WARRANTIES OR REPRESENTATIONS, EXPRESS OR IMPLIED,
 * INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE, AS TO ANY ELEMENT OF THE
 * SOFTWARE OR ANY SUPPORT PROVIDED IN CONNECTION WITH THIS SOFTWARE.
 * In no event shall NetworkCS be responsible for any damages, including
 * but not limited to consequential damages, arising from or relating to
 * any use of the Software or related support.
 *
 * Copyright 1994-1998 Network Computing Services, Inc.
 *
 * Copies of this Software may be made, however, the above copyright
 * notice must be reproduced on all copies.
 *
 *	@(#) $FreeBSD$
 *
 */

/*
 * User configuration and display program
 * --------------------------------------
 *
 * General subroutines
 *
 */

#include <sys/param.h>  
#include <sys/socket.h> 
#include <net/if.h>
#include <netinet/in.h>
#include <netatm/port.h>
#include <netatm/atm.h>
#include <netatm/atm_if.h> 
#include <netatm/atm_sap.h>
#include <netatm/atm_sys.h>
#include <netatm/atm_ioctl.h>
#include <arpa/inet.h>

#include <errno.h>
#include <libatm.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "atm.h"

#ifndef lint
__RCSID("@(#) $FreeBSD$");
#endif


/*
 * Table entry definition
 */
typedef struct {
	int	type;
	const char *name;
} tbl_ent;


/*
 * Table to translate vendor codes to ASCII
 */
static const tbl_ent	vendors[] = {
	{ VENDOR_UNKNOWN,	"Unknown" },
	{ VENDOR_FORE,		"Fore" },
	{ VENDOR_ENI,		"ENI" },
	{ VENDOR_IDT,		"IDT" },
	{ VENDOR_PROSUM,	"ProSum" },
	{ VENDOR_NETGRAPH,	"Netgraph" },
	{ 0,			0 },
};


/*
 * Table to translate adapter codes to ASCII
 */
static const tbl_ent adapter_types[] = {
	{ DEV_UNKNOWN,		"Unknown" },
	{ DEV_FORE_SBA200E,	"SBA-200E" },
	{ DEV_FORE_SBA200,	"SBA-200" },
	{ DEV_FORE_PCA200E,	"PCA-200E" },
	{ DEV_FORE_ESA200E,	"ESA-200E" },
	{ DEV_ENI_155P,		"ENI-155p" },
	{ DEV_IDT_155,		"IDT" },
	{ DEV_PROATM_25,	"PROATM-25" },
	{ DEV_PROATM_155,	"PROATM-155" },
	{ DEV_VATMPIF,		"VATMPIF" },
	{ DEV_FORE_LE25,	"ForeLE-25" },
	{ DEV_FORE_LE155,	"ForeLE-155" },
	{ DEV_IDT_25,		"NICStAR-25" },
	{ DEV_IDTABR_25,	"IDT77252-25" },
	{ DEV_IDTABR_155,	"IDT77252-155" },
	{ DEV_FORE_HE155,	"ForeHE-155" },
	{ DEV_FORE_HE622,	"ForeHE-622" },
	{ 0,			0 },
};

/*
 * Table to translate medium types to ASCII
 */
static const tbl_ent media_types[] = {
	{ MEDIA_UNKNOWN,	"Unknown" },
	{ MEDIA_TAXI_100,	"100 Mbps 4B/5B" },
	{ MEDIA_TAXI_140,	"140 Mbps 4B/5B" },
	{ MEDIA_OC3C,		"OC-3c" },
	{ MEDIA_OC12C,		"OC-12c" },
	{ MEDIA_UTP155,		"155 Mbps UTP" },
	{ MEDIA_UTP25,		"25.6 Mbps UTP" },
	{ MEDIA_VIRTUAL,	"Virtual Link" },
	{ MEDIA_DSL,		"xDSL" },
	{ 0,			0 },
};

/*
 * Table to translate bus types to ASCII
 */
static const tbl_ent bus_types[] = {
	{ BUS_UNKNOWN,	"Unknown" },
	{ BUS_SBUS_B16,	"SBus" },
	{ BUS_SBUS_B32,	"SBus" },
	{ BUS_PCI,	"PCI" },
	{ BUS_EISA,	"EISA" },
	{ BUS_USB,	"USB" },
	{ BUS_VIRTUAL,	"Virtual" },
	{ 0,			0 },
};


/*
 * Get interface vendor name
 *
 * Return a character string with a vendor name, given a vendor code.
 * 
 * Arguments:
 *	vendor	vendor ID
 *
 * Returns:
 *	char *	pointer to a string with the vendor name
 *
 */
const char *
get_vendor(int vendor)
{
	int	i;

	for(i=0; vendors[i].name; i++) {
		if (vendors[i].type == vendor)
			return(vendors[i].name);
	}

	return("-");
}


/*
 * Get adapter type
 *
 * Arguments:
 *	dev	adapter code
 *
 * Returns:
 *	char *	pointer to a string with the adapter type
 *
 */
const char *
get_adapter(int dev)
{
	int	i;

	for(i=0; adapter_types[i].name; i++) {
		if (adapter_types[i].type == dev)
			return(adapter_types[i].name);
	}

	return("-");
}


/*
 * Get communication medium type
 *
 * Arguments:
 *	media	medium code
 *
 * Returns:
 *	char *	pointer to a string with the name of the medium
 *
 */
const char *
get_media_type(int media)
{
	int	i;

	for(i=0; media_types[i].name; i++) {
		if (media_types[i].type == media)
			return(media_types[i].name);
	}

	return("-");
}


/*
 * Get bus type
 *
 * Arguments:
 *	bus	bus type code
 *
 * Returns:
 *	char *	pointer to a string with the bus type
 *
 */
const char *
get_bus_type(int bus)
{
	int	i;

	for(i=0; bus_types[i].name; i++) {
		if (bus_types[i].type == bus)
			return(bus_types[i].name);
	}

	return("-");
}


/*
 * Get adapter ID
 *
 * Get a string giving the adapter's vendor and type.
 *
 * Arguments:
 *	intf	interface name
 *
 * Returns:
 *	char *	pointer to a string identifying the adapter
 *
 */
const char *
get_adapter_name(const char *intf)
{
	size_t buf_len;
	struct atminfreq	air;
	struct air_cfg_rsp	*cfg;
	static char		name[256];

	/*
	 * Initialize
	 */
	bzero(&air, sizeof(air));
	bzero(name, sizeof(name));

	/*
	 * Get configuration information from the kernel
	 */
	air.air_opcode = AIOCS_INF_CFG;
	strcpy(air.air_cfg_intf, intf);
	buf_len = do_info_ioctl(&air, sizeof(struct air_cfg_rsp));
	if (buf_len < sizeof(struct air_cfg_rsp))
		return("-");
	cfg = (struct air_cfg_rsp *)(void *)air.air_buf_addr;

	/*
	 * Build a string describing the adapter
	 */
	strcpy(name, get_vendor(cfg->acp_vendor));
	strcat(name, " ");
	strcat(name, get_adapter(cfg->acp_device));

	free(cfg);

	return(name);
}


/*
 * Format a MAC address into a string
 * 
 * Arguments:
 *	addr	pointer to a MAC address
 *
 * Returns:
 *		the address of a string representing the MAC address
 *
 */
const char *
format_mac_addr(const Mac_addr *addr)
{
	static char	str[256];

	/*
	 * Check for null pointer
	 */
	if (!addr)
		return("-");

	/*
	 * Clear the returned string
	 */
	bzero(str, sizeof(str));

	/*
	 * Format the address
	 */
	sprintf(str, "%02x:%02x:%02x:%02x:%02x:%02x",
			addr->ma_data[0],
			addr->ma_data[1],
			addr->ma_data[2],
			addr->ma_data[3],
			addr->ma_data[4],
			addr->ma_data[5]);

	return(str);
}


/*
 * Parse an IP prefix designation in the form nnn.nnn.nnn.nnn/mm
 *
 * Arguments:
 *	cp	pointer to prefix designation string
 *	op	pointer to a pair of in_addrs for the result
 *
 * Returns:
 *	0	success
 *	-1	prefix was invalid
 *
 */
int
parse_ip_prefix(const char *cp, struct in_addr *op)
{
	int		len;
	char		*mp;
	struct in_addr	ip_addr;

	static u_long	masks[33] = {
		0x0,
		0x80000000,
		0xc0000000,
		0xe0000000,
		0xf0000000,
		0xf8000000,
		0xfc000000,
		0xfe000000,
		0xff000000,
		0xff800000,
		0xffc00000,
		0xffe00000,
		0xfff00000,
		0xfff80000,
		0xfffc0000,
		0xfffe0000,
		0xffff0000,
		0xffff8000,
		0xffffc000,
		0xffffe000,
		0xfffff000,
		0xfffff800,
		0xfffffc00,
		0xfffffe00,
		0xffffff00,
		0xffffff80,
		0xffffffc0,
		0xffffffe0,
		0xfffffff0,
		0xfffffff8,
		0xfffffffc,
		0xfffffffe,
		0xffffffff
	};

	/*
	 * Find the slash that marks the start of the mask
	 */
	mp = strchr(cp, '/');
	if (mp) {
		*mp = '\0';
		mp++;
	}

	/*
	 * Convert the IP-address part of the prefix
	 */
	ip_addr.s_addr = inet_addr(cp);
	if (ip_addr.s_addr == INADDR_NONE)
		return(-1);

	/*
	 * Set the default mask length
	 */
	if (IN_CLASSA(ntohl(ip_addr.s_addr)))
		len = 8;
	else if (IN_CLASSB(ntohl(ip_addr.s_addr)))
		len = 16;
	else if (IN_CLASSC(ntohl(ip_addr.s_addr)))
		len = 24;
	else
		return(-1);

	/*
	 * Get the mask length
	 */
	if (mp) {
		len = atoi(mp);
		if (len < 1 || len > 32)
			return(-1);
	}

	/*
	 * Select the mask and copy the IP address into the
	 * result buffer, ANDing it with the mask
	 */
	op[1].s_addr = htonl(masks[len]);
	op[0].s_addr = ip_addr.s_addr & op[1].s_addr;

	return(0);
}


/*
 * Compress a list of IP network prefixes
 *
 * Arguments:
 *	ipp	pointer to list of IP address/mask pairs
 *	ipc	length of list
 *
 * Returns:
 *	length of compressed list
 *
 */
size_t
compress_prefix_list(struct in_addr *ipp, size_t ilen)
{
	u_int		i, j, n;
	struct in_addr	*ip1, *ip2, *m1, *m2;

	/*
	 * Figure out how many pairs there are
	 */
	n = ilen / (sizeof(struct in_addr) * 2);

	/*
	 * Check each pair of address/mask pairs to make sure
	 * none contains the other
	 */
	for (i = 0; i < n; i++) {
		ip1 = &ipp[i*2];
		m1 = &ipp[i*2+1];

		/*
		 * If we've already eliminated this address,
		 * skip the checks
		 */
		if (ip1->s_addr == 0)
			continue;

		/*
		 * Try all possible second members of the pair
		 */
		for (j = i + 1; j < n; j++) {
			ip2 = &ipp[j*2];
			m2 = &ipp[j*2+1];

			/*
			 * If we've already eliminated the second
			 * address, just skip the checks
			 */
			if (ip2->s_addr == 0)
				continue;

			/*
			 * Compare the address/mask pairs
			 */
			if (m1->s_addr == m2->s_addr) {
				/*
				 * Masks are equal
				 */
				if (ip1->s_addr == ip2->s_addr) {
					ip2->s_addr = 0;
					m2->s_addr = 0;
				}
			} else if (ntohl(m1->s_addr) <
					ntohl(m2->s_addr)) {
				/*
				 * m1 is shorter
				 */
				if ((ip2->s_addr & m1->s_addr) ==
						ip1->s_addr) {
					ip2->s_addr = 0;
					m2->s_addr = 0;
				}
			} else {
				/*
				 * m1 is longer
				 */
				if ((ip1->s_addr & m2->s_addr) ==
						ip2->s_addr) {
					ip1->s_addr = 0;
					m1->s_addr = 0;
					break;
				}
			}
		}
	}

	/*
	 * Now pull up the list, eliminating zeroed entries
	 */
	for (i = 0, j = 0; i < n; i++) {
		ip1 = &ipp[i*2];
		m1 = &ipp[i*2+1];
		ip2 = &ipp[j*2];
		m2 = &ipp[j*2+1];
		if (ip1->s_addr != 0) {
			if (i != j) {
				*ip2 = *ip1;
				*m2 = *m1;
			}
			j++;
		}
	}

	return(j * sizeof(struct in_addr) * 2);
}


/*
 * Make sure a user-supplied parameter is a valid network interface
 * name
 *
 * When a socket call fails, print an error message and exit
 * 
 * Arguments:
 *	nif	pointer to network interface name
 *
 * Returns:
 *	none	exits if name is not valid
 *
 */
void
check_netif_name(const char *nif)
{
	int	rc;

	/*
	 * Look up the name in the kernel
	 */
	rc = verify_nif_name(nif);

	/*
	 * Check the result
	 */
	if (rc > 0) {
		/*
		 * Name is OK
		 */
		return;
	} else if (rc == 0) {
		/*
		 * Name is not valid
		 */
		fprintf(stderr, "%s: Invalid network interface name %s\n",
				prog, nif);
	} else {
		/*
		 * Error performing IOCTL
		 */
		fprintf(stderr, "%s: ", prog);
		switch(errno) {
		case ENOPROTOOPT:
		case EOPNOTSUPP:
			perror("Internal error");
			break;
		case ENXIO:
			fprintf(stderr, "%s is not an ATM device\n",
					nif);
			break;
		default:
			perror("ioctl (AIOCINFO)");
			break;
		}
	}

	exit(1);
}


/*
 * Socket error handler
 *
 * When a socket call fails, print an error message and exit
 * 
 * Arguments:
 *	err	an errno describing the error
 *
 * Returns:
 *	none
 *
 */
void
sock_error(int err)
{
	fprintf(stderr, "%s: ", prog);

	switch (err) {

	case EPROTONOSUPPORT:
		fprintf(stderr, "ATM protocol support not loaded\n");
		break;

	default:
		perror("socket");
		break;
	}

	exit(1);
}
