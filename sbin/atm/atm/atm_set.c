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
 *	@(#) $FreeBSD: src/sbin/atm/atm/atm_set.c,v 1.3.2.1 2000/07/01 06:02:14 ps Exp $
 *
 */

/*
 * User configuration and display program
 * --------------------------------------
 *
 * Routines for "set" subcommand
 *
 */

#include <sys/param.h>  
#include <sys/socket.h> 
#include <sys/sockio.h> 
#include <net/if.h>
#include <netinet/in.h>
#include <netatm/port.h>
#include <netatm/atm.h>
#include <netatm/atm_if.h> 
#include <netatm/atm_sap.h>
#include <netatm/atm_sys.h>
#include <netatm/atm_ioctl.h>

#include <errno.h>
#include <libatm.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "atm.h"

#ifndef lint
__RCSID("@(#) $FreeBSD: src/sbin/atm/atm/atm_set.c,v 1.3.2.1 2000/07/01 06:02:14 ps Exp $");
#endif


/*
 * Process ATM ARP server set command
 *
 * Command format:
 *	atm set arpserver <interface_name> <atm-address> <IP prefix> ...
 *
 * Arguments:
 *	argc	number of arguments to command
 *	argv	pointer to argument strings
 *	cmdp	pointer to command description
 *
 * Returns:
 *	none
 *
 */
void
set_arpserver(argc, argv, cmdp)
	int		argc;
	char		**argv;
	struct cmd	*cmdp;
{
	int			i, len, prefix_len = 0, rc, s;
	char			*intf;
	Atm_addr		server;
	struct sockaddr_in	*lis;
	struct sockaddr_in	if_mask;
	struct atmsetreq	asr;
	struct atminfreq	air;
	struct air_netif_rsp	*int_info;
	struct {
		struct in_addr	ip_addr;
		struct in_addr	ip_mask;
	} prefix_buf[64];;

	/*
	 * Validate interface name
	 */
	check_netif_name(argv[0]);
	intf = argv[0];
	argc--; argv++;

	/*
	 * Get the ARP server's ATM address
	 */
	UM_ZERO(&server, sizeof(server));
	if (strcasecmp(argv[0], "local")) {
		/*
		 * ARP server NSAP address is provided
		 */
		server.address_format = T_ATM_ENDSYS_ADDR;
		server.address_length = sizeof(Atm_addr_nsap);
		if (get_hex_atm_addr(argv[0],
					(u_char *)server.address,
					sizeof(Atm_addr_nsap)) !=
				sizeof(Atm_addr_nsap)) {
			fprintf(stderr, "%s: Invalid ARP server address\n",
					prog);
			exit(1);
		}
		if (argc > 1) {
			fprintf(stderr, "%s: Invalid number of arguments\n",
					prog);
			exit(1);
		}
		prefix_len = 0;
	} else {
		argc--; argv++;

		/*
		 * This host is the ARP server
		 */
		server.address_format = T_ATM_ABSENT;
		server.address_length = 0;

		/*
		 * Get interface information from the kernel.  We need
		 * to get the IP address and the subnet mask associated
		 * with the network interface and insert them into the
		 * list of permitted LIS prefixes.
		 */
		len = sizeof(struct air_netif_rsp);
		UM_ZERO(&air, sizeof(air));
		air.air_opcode = AIOCS_INF_NIF;
		strcpy(air.air_int_intf, intf);
		len = do_info_ioctl(&air, len);
		if (len < 0) {
			fprintf(stderr, "%s: ", prog);
			switch (errno) {
			case ENOPROTOOPT:
			case EOPNOTSUPP:
				perror("Internal error");
				break;
			case ENXIO:
				fprintf(stderr, "%s is not an ATM device\n",
						intf);
				break;
			default:
				perror("ioctl (AIOCINFO)");
				break;
			}
			exit(1);
		}
		int_info = (struct air_netif_rsp *) air.air_buf_addr;
		lis = (struct sockaddr_in *)&int_info->anp_proto_addr;
		prefix_buf[0].ip_addr = lis->sin_addr;
		UM_FREE(int_info);
	
		rc = get_subnet_mask(intf, &if_mask);
		if (rc) {
			fprintf(stderr, "%s: Can't get subnet mask for %s\n",
					prog, intf);
		}
		prefix_buf[0].ip_mask = if_mask.sin_addr;
		prefix_buf[0].ip_addr.s_addr &=
				prefix_buf[0].ip_mask.s_addr;

		/*
		 * Get the prefixes of the LISs that we'll support
		 */
		for (i = 1; argc; i++, argc--, argv++) {
			rc = parse_ip_prefix(argv[0],
					(struct in_addr *)&prefix_buf[i]);
			if (rc != 0) {
				fprintf(stderr, "%s: Invalid IP prefix value \'%s\'\n",
					prog, argv[0]);
				exit(1);
			}
		}

		/*
		 * Compress the prefix list
		 */
		prefix_len = compress_prefix_list((struct in_addr *)prefix_buf,
				i * sizeof(struct in_addr) * 2);
	}

	/*
	 * Build ioctl request
	 */
	UM_ZERO(&asr, sizeof(asr));
	asr.asr_opcode = AIOCS_SET_ASV;
	strncpy(asr.asr_arp_intf, intf, sizeof(asr.asr_arp_intf));
	asr.asr_arp_addr = server;
	asr.asr_arp_subaddr.address_format = T_ATM_ABSENT;
	asr.asr_arp_subaddr.address_length = 0;
	if (prefix_len)
		asr.asr_arp_pbuf = (caddr_t)prefix_buf;
	else
		asr.asr_arp_pbuf = (caddr_t)0;
	asr.asr_arp_plen = prefix_len;

	/*
	 * Pass the new ARP server address to the kernel
	 */
	s = socket(AF_ATM, SOCK_DGRAM, 0);
	if (s < 0) {
		sock_error(errno);
	}
	if (ioctl(s, AIOCSET, (caddr_t)&asr) < 0) {
		fprintf(stderr, "%s: ", prog);
		switch (errno) {
		case EOPNOTSUPP:
		case EPROTONOSUPPORT:
			perror("Internal error");
			break;
		case EINVAL:
			fprintf(stderr, "Invalid parameter\n");
			break;
		case ENOMEM:
			fprintf(stderr, "Kernel memory exhausted\n");
			break;
		case ENETDOWN:
			fprintf(stderr, "ATM network is inoperable\n");
			break;
		case EPERM:
			fprintf(stderr, "Must be super user to use set subcommand\n");
			break;
		case ENXIO:
			fprintf(stderr, "%s is not an ATM interface\n", intf);
			break;
		case ENOENT:
			fprintf(stderr, "Signalling manager not attached\n");
			break;
		case ENOPROTOOPT:
			fprintf(stderr,
				"%s does not have an IP address configured\n",
				intf);
			break;
		default:
			perror("Ioctl (AIOCSET) ARPSERVER address");
			break;
		}
		exit(1);
	}

	(void)close(s);
}


/*
 * Process set MAC address command
 *
 * Command format:
 *	atm set mac <interface_name> <MAC address>
 *
 * Arguments:
 *	argc	number of remaining arguments to command
 *	argv	pointer to remaining argument strings
 *	cmdp	pointer to command description
 *
 * Returns:
 *	none
 *
 */
void
set_macaddr(argc, argv, cmdp)
	int		argc;
	char		**argv;
	struct cmd	*cmdp;
{
	int			s;
	char			*intf;
	struct mac_addr		mac;
	struct atmsetreq	asr;

	/*
	 * Validate interface name
	 */
	if (strlen(argv[0]) > sizeof(asr.asr_mac_intf) - 1) {
		fprintf(stderr, "%s: Illegal interface name\n", prog);
		exit(1);
	}
	intf = argv[0];
	argc--; argv++;

	/*
	 * Get the MAC address provided by the user
	 */
	if (get_hex_atm_addr(argv[0], (u_char *)&mac, sizeof(mac)) !=
			sizeof(mac)) {
		fprintf(stderr, "%s: Invalid MAC address\n", prog);
		exit(1);
	}

	/*
	 * Build ioctl request
	 */
	asr.asr_opcode = AIOCS_SET_MAC;
	strncpy(asr.asr_mac_intf, intf, sizeof(asr.asr_mac_intf));
	UM_COPY(&mac, &asr.asr_mac_addr, sizeof(asr.asr_mac_addr));

	/*
	 * Pass the new address to the kernel
	 */
	s = socket(AF_ATM, SOCK_DGRAM, 0);
	if (s < 0) {
		sock_error(errno);
	}
	if (ioctl(s, AIOCSET, (caddr_t)&asr) < 0) {
		fprintf(stderr, "%s: ", prog);
		switch (errno) {
		case EOPNOTSUPP:
		case EPROTONOSUPPORT:
			perror("Internal error");
			break;
		case EADDRINUSE:
			fprintf(stderr, "Interface must be detached to set MAC addres\n");
			break;
		case EINVAL:
			fprintf(stderr, "Invalid parameter\n");
			break;
		case ENOMEM:
			fprintf(stderr, "Kernel memory exhausted\n");
			break;
		case ENETDOWN:
			fprintf(stderr, "ATM network is inoperable\n");
			break;
		case EPERM:
			fprintf(stderr, "Must be super user to use set subcommand\n");
			break;
		case ENXIO:
			fprintf(stderr, "%s is not an ATM device\n",
					argv[0]);
			break;
		default:
			perror("Ioctl (AIOCSET) MAC address");
			break;
		}
		exit(1);
	}

	(void)close(s);
}


/*
 * Process network interface set command
 *
 * Command format:
 *	atm set netif <interface_name> <prefix_name> <count>
 *
 * Arguments:
 *	argc	number of arguments to command
 *	argv	pointer to argument strings
 *	cmdp	pointer to command description
 *
 * Returns:
 *	none
 *
 */
void
set_netif(argc, argv, cmdp)
	int		argc;
	char		**argv;
	struct cmd	*cmdp;
{
	struct atmsetreq	anr;
	char			str[16];
	char			*cp;
	int			nifs, s;

	/*
	 * Set IOCTL opcode
	 */
	anr.asr_opcode = AIOCS_SET_NIF;

	/*
	 * Validate interface name
	 */
	if (strlen(argv[0]) > sizeof(anr.asr_nif_intf) - 1) {
		fprintf(stderr, "%s: Illegal interface name\n", prog);
		exit(1);
	}
	strcpy(anr.asr_nif_intf, argv[0]);
	argc--; argv++;

	/*
	 * Validate network interface name prefix
	 */
	if ((strlen(argv[0]) > sizeof(anr.asr_nif_pref) - 1) ||
			(strpbrk(argv[0], "0123456789"))) {
		fprintf(stderr, "%s: Illegal network interface prefix\n", prog);
		exit(1);
	}
	strcpy(anr.asr_nif_pref, argv[0]);
	argc--; argv++;

	/*
	 * Validate interface count
	 */
	nifs = (int) strtol(argv[0], &cp, 0);
	if ((*cp != '\0') || (nifs < 0) || (nifs > MAX_NIFS)) {
		fprintf(stderr, "%s: Invalid interface count\n", prog);
		exit(1);
	}
	anr.asr_nif_cnt = nifs;

	/*
	 * Make sure the resulting name won't be too long
	 */
	sprintf(str, "%d", nifs - 1);
	if ((strlen(str) + strlen(anr.asr_nif_pref)) >
			sizeof(anr.asr_nif_intf) - 1) {
		fprintf(stderr, "%s: Network interface prefix too long\n", prog);
		exit(1);
	}

	/*
	 * Tell the kernel to do it
	 */
	s = socket(AF_ATM, SOCK_DGRAM, 0);
	if (s < 0) {
		sock_error(errno);
	}
	if (ioctl(s, AIOCSET, (caddr_t)&anr) < 0) {
		fprintf(stderr, "%s: ", prog);
		perror("ioctl (AIOCSET) set NIF");
		exit(1);
	}
	(void)close(s);
}


/*
 * Process set NSAP prefix command
 *
 * Command format:
 *	atm set nsap <interface_name> <NSAP prefix>
 *
 * Arguments:
 *	argc	number of remaining arguments to command
 *	argv	pointer to remaining argument strings
 *	cmdp	pointer to command description
 *
 * Returns:
 *	none
 *
 */
void
set_prefix(argc, argv, cmdp)
	int		argc;
	char		**argv;
	struct cmd	*cmdp;
{
	int			s;
	char			*intf;
	u_char			prefix[13];
	struct atmsetreq	asr;

	/*
	 * Validate interface name
	 */
	if (strlen(argv[0]) > sizeof(asr.asr_prf_intf) - 1) {
		fprintf(stderr, "%s: Illegal interface name\n", prog);
		exit(1);
	}
	intf = argv[0];
	argc--; argv++;

	/*
	 * Get the prefix provided by the user
	 */
	if (get_hex_atm_addr(argv[0], prefix, sizeof(prefix)) !=
			sizeof(prefix)) {
		fprintf(stderr, "%s: Invalid NSAP prefix\n", prog);
		exit(1);
	}

	/*
	 * Build ioctl request
	 */
	asr.asr_opcode = AIOCS_SET_PRF;
	strncpy(asr.asr_prf_intf, intf, sizeof(asr.asr_prf_intf));
	UM_COPY(prefix, asr.asr_prf_pref, sizeof(asr.asr_prf_pref));

	/*
	 * Pass the new prefix to the kernel
	 */
	s = socket(AF_ATM, SOCK_DGRAM, 0);
	if (s < 0) {
		sock_error(errno);
	}
	if (ioctl(s, AIOCSET, (caddr_t)&asr) < 0) {
		fprintf(stderr, "%s: ", prog);
		switch (errno) {
		case EOPNOTSUPP:
		case EPROTONOSUPPORT:
			perror("Internal error");
			break;
		case EALREADY:
			fprintf(stderr, "NSAP prefix is already set\n");
			break;
		case EINVAL:
			fprintf(stderr, "Invalid parameter\n");
			break;
		case ENOMEM:
			fprintf(stderr, "Kernel memory exhausted\n");
			break;
		case ENETDOWN:
			fprintf(stderr, "ATM network is inoperable\n");
			break;
		case EPERM:
			fprintf(stderr, "Must be super user to use set subcommand\n");
			break;
		case ENXIO:
			fprintf(stderr, "%s is not an ATM device\n",
					argv[0]);
			break;
		default:
			perror("Ioctl (AIOCSET) NSAP prefix");
			break;
		}
		exit(1);
	}

	(void)close(s);
}
