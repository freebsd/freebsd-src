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
 * Routines for "show" subcommand
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
#include <netatm/atm_vc.h>
#include <netatm/atm_ioctl.h>

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
 * Local functions
 */
static int	vcc_compare(const void *, const void *);
static int	ip_vcc_compare(const void *, const void *);
static int	arp_compare(const void *, const void *);


/*
 * Process show ARP command
 * 
 * Command format: 
 *	atm show ARP [<ip-addr>]
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
show_arp(argc, argv, cmdp)
	int		argc;
	char		**argv;
	struct cmd	*cmdp;
{
	int			buf_len, arp_info_len;
	struct atminfreq	air;
	struct air_arp_rsp	*arp_info, *arp_info_base;
	struct sockaddr_in	*sin;
	union {
		struct sockaddr_in	sin;
		struct sockaddr		sa;
	} host_addr;

	/*
	 * Get IP address of specified host name
	 */
	bzero(&host_addr, sizeof(host_addr));
	host_addr.sa.sa_family = AF_INET;
	if (argc) {
		sin = get_ip_addr(argv[0]);
		if (!sin) {
			fprintf(stderr, "%s: host \'%s\' not found\n",
					prog, argv[0]);
			exit(1);
		}
		host_addr.sin.sin_addr.s_addr = sin->sin_addr.s_addr;
	} else {
		host_addr.sin.sin_addr.s_addr = INADDR_ANY;
	}

	/*
	 * Get ARP information from the kernel
	 */
	bzero(&air, sizeof(air));
	buf_len = sizeof(struct air_arp_rsp) * 10;
	air.air_opcode = AIOCS_INF_ARP;
	air.air_arp_addr = host_addr.sa;
	arp_info_len = do_info_ioctl(&air, buf_len);
	if (arp_info_len < 0) {
		fprintf(stderr, "%s: ", prog);
		switch (errno) {
		case ENOPROTOOPT:
		case EOPNOTSUPP:
			perror("Internal error");
			break;
		case ENXIO:
			fprintf(stderr, "not an ATM device\n");
			break;
		default:
			perror("ioctl (AIOCINFO)");
			break;
		}
		exit(1);
	}
	arp_info_base = arp_info =
			(struct air_arp_rsp *) air.air_buf_addr;

	/*
	 * Sort the ARP table
	 */
	qsort((void *) arp_info,
			arp_info_len / sizeof(struct air_arp_rsp),
			sizeof(struct air_arp_rsp),
			arp_compare);

	/*
	 * Print the relevant information
	 */
	while (arp_info_len > 0) {
		print_arp_info(arp_info);
		arp_info++;
		arp_info_len -= sizeof(struct air_arp_rsp);
	}

	/*
	 * Release the information from the kernel
	 */
	free(arp_info_base);
}


/*
 * Process show ATM ARP server command
 * 
 * Command format: 
 *	atm show arpserver [<interface-name>]
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
show_arpserv(argc, argv, cmdp)
	int		argc;
	char		**argv;
	struct cmd	*cmdp;
{
	int	asrv_info_len, buf_len = sizeof(struct air_asrv_rsp) * 3;
	struct atminfreq	air;
	struct air_asrv_rsp	*asrv_info, *asrv_info_base;

	/*
	 * Validate interface name
	 */
	bzero(air.air_int_intf, sizeof(air.air_int_intf));
	if (argc) {
		if (strlen(argv[0]) > IFNAMSIZ - 1) {
			fprintf(stderr, "%s: Illegal interface name\n",
					prog);
			exit(1);
		}
		strcpy(air.air_int_intf, argv[0]);
		argc--; argv++;
	}

	/*
	 * Get interface information from the kernel
	 */
	air.air_opcode = AIOCS_INF_ASV;
	buf_len = do_info_ioctl(&air, buf_len);
	if (buf_len < 0) {
		fprintf(stderr, "%s: ", prog);
		switch (errno) {
		case ENOPROTOOPT:
		case EOPNOTSUPP:
			perror("Internal error");
			break;
		case ENXIO:
			fprintf(stderr, "%s is not an ATM device\n",
					argv[0]);
			break;
		default:
			perror("ioctl (AIOCINFO)");
			break;
		}
		exit(1);
	}

	/*
	 * Print the interface information
	 */
	asrv_info_base = asrv_info =
			(struct air_asrv_rsp *) air.air_buf_addr;
	for (; buf_len >= sizeof(struct air_asrv_rsp);
			asrv_info = (struct air_asrv_rsp *)
				((u_long)asrv_info + asrv_info_len),
			buf_len -= asrv_info_len) {
		print_asrv_info(asrv_info);
		asrv_info_len = sizeof(struct air_asrv_rsp) +
				asrv_info->asp_nprefix *
				sizeof(struct in_addr) * 2;
	}
	free(asrv_info_base);
}


/*
 * Process show ATM adapter configuration command
 * 
 * Command format: 
 *	atm show config [<interface-name>]
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
show_config(argc, argv, cmdp)
	int		argc;
	char		**argv;
	struct cmd	*cmdp;
{
	int	buf_len = sizeof(struct air_asrv_rsp) * 3;
	struct atminfreq	air;
	struct air_cfg_rsp	*cfg_info, *cfg_info_base;

	/*
	 * Validate interface name
	 */
	bzero(air.air_cfg_intf, sizeof(air.air_cfg_intf));
	if (argc) {
		if (strlen(argv[0]) > IFNAMSIZ - 1) {
			fprintf(stderr, "%s: Illegal interface name\n",
					prog);
			exit(1);
		}
		strcpy(air.air_cfg_intf, argv[0]);
		argc--; argv++;
	}

	/*
	 * Get configuration information from the kernel
	 */
	air.air_opcode = AIOCS_INF_CFG;
	buf_len = do_info_ioctl(&air, buf_len);
	if (buf_len < 0) {
		fprintf(stderr, "%s: ", prog);
		switch (errno) {
		case ENOPROTOOPT:
		case EOPNOTSUPP:
			perror("Internal error");
			break;
		case ENXIO:
			fprintf(stderr, "%s is not an ATM device\n",
					argv[0]);
			break;
		default:
			perror("ioctl (AIOCINFO)");
			break;
		}
		exit(1);
	}

	/*
	 * Print the interface information
	 */
	cfg_info_base = cfg_info =
			(struct air_cfg_rsp *) air.air_buf_addr;
	for (; buf_len >= sizeof(struct air_cfg_rsp); cfg_info++,
			buf_len -= sizeof(struct air_cfg_rsp)) {
		print_cfg_info(cfg_info);
	}
	free(cfg_info_base);
}


/*
 * Process show interface command
 * 
 * Command format: 
 *	atm show interface [<interface-name>]
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
show_intf(argc, argv, cmdp)
	int		argc;
	char		**argv;
	struct cmd	*cmdp;
{
	int	buf_len = sizeof(struct air_int_rsp) * 3;
	struct atminfreq	air;
	struct air_int_rsp	*int_info, *int_info_base;

	/*
	 * Validate interface name
	 */
	bzero(&air, sizeof(air));
	if (argc) {
		if (strlen(argv[0]) > IFNAMSIZ - 1) {
			fprintf(stderr, "%s: Illegal interface name\n",
					prog);
			exit(1);
		}
		strcpy(air.air_int_intf, argv[0]);
		argc--; argv++;
	}

	/*
	 * Get interface information from the kernel
	 */
	air.air_opcode = AIOCS_INF_INT;
	buf_len = do_info_ioctl(&air, buf_len);
	if (buf_len < 0) {
		fprintf(stderr, "%s: ", prog);
		switch (errno) {
		case ENOPROTOOPT:
		case EOPNOTSUPP:
			perror("Internal error");
			break;
		case ENXIO:
			fprintf(stderr, "%s is not an ATM device\n",
					argv[0]);
			break;
		default:
			perror("ioctl (AIOCINFO)");
			break;
		}
		exit(1);
	}

	/*
	 * Print the interface information
	 */
	int_info_base = int_info =
			(struct air_int_rsp *) air.air_buf_addr;
	for (; buf_len >= sizeof(struct air_int_rsp); int_info++,
			buf_len -= sizeof(struct air_int_rsp)) {
		print_intf_info(int_info);
	}
	free(int_info_base);
}


/*
 * Process show IP VCCs command
 * 
 * Command format: 
 *	atm show ipvcc [<ip-addr>]
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
show_ip_vcc(argc, argv, cmdp)
	int		argc;
	char		**argv;
	struct cmd	*cmdp;
{
	int			buf_len, ip_info_len, rc;
	char			*if_name = (char *)0;
	struct atminfreq	air;
	struct air_ip_vcc_rsp	*ip_info, *ip_info_base;
	struct sockaddr_in	*sin;
	union {
		struct sockaddr_in	sin;
		struct sockaddr		sa;
	} host_addr;

	/*
	 * First parameter can be a netif name, an IP host name, or
	 * an IP address.  Figure out which it is.
	 */
	bzero(&host_addr, sizeof(host_addr));
	host_addr.sa.sa_family = AF_INET;
	if (argc) {
		rc = verify_nif_name(argv[0]);
		if (rc < 0) {
			/*
			 * Error occured
			 */
			fprintf(stderr, "%s: ", prog);
			switch (errno) {
			case ENOPROTOOPT:
			case EOPNOTSUPP:
				perror("Internal error");
				break;
			case ENXIO:
				fprintf(stderr, "%s is not an ATM device\n",
						argv[0]);
				break;
			default:
				perror("ioctl (AIOCINFO)");
				break;
			}
			exit(1);
		} else if (rc > 0) {
			/*
			 * Parameter is a valid netif name
			 */
			if_name = argv[0];
		} else {
			/*
			 * Get IP address of specified host name
			 */
			sin = get_ip_addr(argv[0]);
			host_addr.sin.sin_addr.s_addr =
					sin->sin_addr.s_addr;
		}
	} else {
		host_addr.sin.sin_addr.s_addr = INADDR_ANY;
	}

	/*
	 * Get IP map information from the kernel
	 */
	buf_len = sizeof(struct air_ip_vcc_rsp) * 10;
	air.air_opcode = AIOCS_INF_IPM;
	air.air_ip_addr = host_addr.sa;
	ip_info_len = do_info_ioctl(&air, buf_len);
	if (ip_info_len < 0) {
		fprintf(stderr, "%s: ", prog);
		switch (errno) {
		case ENOPROTOOPT:
		case EOPNOTSUPP:
			perror("Internal error");
			break;
		case ENXIO:
			fprintf(stderr, "not an ATM device\n");
			break;
		default:
			perror("ioctl (AIOCINFO)");
			break;
		}
		exit(1);
	}
	ip_info_base = ip_info =
			(struct air_ip_vcc_rsp *) air.air_buf_addr;

	/*
	 * Sort the information
	 */
	qsort((void *) ip_info,
			ip_info_len / sizeof(struct air_ip_vcc_rsp),
			sizeof(struct air_ip_vcc_rsp),
			ip_vcc_compare);

	/*
	 * Print the relevant information
	 */
	while (ip_info_len>0) {
		if (!if_name || !strcmp(if_name, ip_info->aip_intf)) {
			print_ip_vcc_info(ip_info);
		}
		ip_info++;
		ip_info_len -= sizeof(struct air_ip_vcc_rsp);
	}

	/*
	 * Release the information from the kernel
	 */
	free(ip_info_base);

}


/*
 * Process show network interface command
 * 
 * Command format: 
 *	atm show netif [<netif>]
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
show_netif(argc, argv, cmdp)
	int		argc;
	char		**argv;
	struct cmd	*cmdp;
{
	int	buf_len = sizeof(struct air_netif_rsp) * 3;
	struct atminfreq	air;
	struct air_netif_rsp	*int_info, *int_info_base;

	/*
	 * Validate network interface name
	 */
	bzero(air.air_int_intf, sizeof(air.air_int_intf));
	if (argc) {
		if (strlen(argv[0]) > IFNAMSIZ - 1) {
			fprintf(stderr, "%s: Illegal interface name\n", prog);
			exit(1);
		}
		strcpy(air.air_int_intf, argv[0]);
		argc--; argv++;
	}

	/*
	 * Get network interface information from the kernel
	 */
	air.air_opcode = AIOCS_INF_NIF;
	buf_len = do_info_ioctl(&air, buf_len);
	if (buf_len < 0) {
		fprintf(stderr, "%s: ", prog);
		switch (errno) {
		case ENOPROTOOPT:
		case EOPNOTSUPP:
			perror("Internal error");
			break;
		case ENXIO:
			fprintf(stderr, "%s is not an ATM device\n",
					argv[0]);
			break;
		default:
			perror("ioctl (AIOCINFO)");
			break;
		}
		exit(1);
	}

	/*
	 * Print the network interface information
	 */
	int_info_base = int_info =
			(struct air_netif_rsp *) air.air_buf_addr;
	for (; buf_len >= sizeof(struct air_netif_rsp); int_info++,
			buf_len -= sizeof(struct air_netif_rsp)) {
		print_netif_info(int_info);
	}
	free(int_info_base);
}


/*
 * Process interface statistics command
 * 
 * Command format: 
 *	atm show stats interface [<interface-name>]
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
show_intf_stats(argc, argv, cmdp)
	int		argc;
	char		**argv;
	struct cmd	*cmdp;
{
	int			buf_len;
	char			intf[IFNAMSIZ];
	struct atminfreq	air;
	struct air_phy_stat_rsp	*pstat_info, *pstat_info_base;
	struct air_cfg_rsp	*cfg_info;

	/*
	 * Validate interface name
	 */
	bzero(intf, sizeof(intf));
	if (argc) {
		if (strlen(argv[0]) > IFNAMSIZ - 1) {
			fprintf(stderr, "%s: Illegal interface name\n",
					prog);
			exit(1);
		}
		strcpy(intf, argv[0]);
		argc--; argv++;
	}

	/*
	 * If there are parameters remaining, the request is for
	 * vendor-specific adaptor statistics
	 */
	if (argc) {
		/*
		 * Get adapter configuration information
		 */
		buf_len = sizeof(struct air_cfg_rsp);
		air.air_opcode = AIOCS_INF_CFG;
		strcpy(air.air_cfg_intf, intf);
		buf_len = do_info_ioctl(&air, buf_len);
		if (buf_len < 0) {
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
		cfg_info = (struct air_cfg_rsp *)air.air_buf_addr;

		/*
		 * Call the appropriate vendor-specific routine
		 */
		switch(cfg_info->acp_vendor) {
		case VENDOR_FORE:
			show_fore200_stats(intf, argc, argv);
			break;
		case VENDOR_ENI:
			show_eni_stats(intf, argc, argv);
			break;
		default:
			fprintf(stderr, "%s: Unknown adapter vendor\n",
					prog);
			break;
		}

		free(cfg_info);
	} else {
		/*
		 * Get generic interface statistics
		 */
		buf_len = sizeof(struct air_phy_stat_rsp) * 3;
		air.air_opcode = AIOCS_INF_PIS;
		strcpy(air.air_physt_intf, intf);
		buf_len = do_info_ioctl(&air, buf_len);
		if (buf_len < 0) {
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

		/*
		 * Display the interface statistics
		 */
		pstat_info_base = pstat_info =
				(struct air_phy_stat_rsp *)air.air_buf_addr;
		for (; buf_len >= sizeof(struct air_phy_stat_rsp);
				pstat_info++,
				buf_len-=sizeof(struct air_phy_stat_rsp)) {
			print_intf_stats(pstat_info);
		}
		free((caddr_t)pstat_info_base);
	}
}


/*
 * Process VCC statistics command
 * 
 * Command format: 
 *	atm show stats VCC [<interface-name> [<vpi> [<vci>]]] 
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
show_vcc_stats(argc, argv, cmdp)
	int		argc;
	char		**argv;
	struct cmd	*cmdp;
{
	int	vcc_info_len;
	int	vpi = -1, vci = -1;
	char	*cp, *intf = NULL;
	struct air_vcc_rsp	*vcc_info, *vcc_info_base;

	/*
	 * Validate interface name
	 */
	if (argc) {
		if (strlen(argv[0]) > IFNAMSIZ - 1) {
			fprintf(stderr, "%s: Illegal interface name\n",
					prog);
			exit(1);
		}
		intf = argv[0];
		argc--; argv++;
	}

	/*
	 * Validate VPI value
	 */
	if (argc) {
		vpi = strtol(argv[0], &cp, 0);
		if ((*cp != '\0') || (vpi < 0) || (vpi >= 1 << 8)) {
			fprintf(stderr, "%s: Invalid VPI value\n", prog);
			exit(1);
		}
		argc--; argv++;
	}

	/*
	 * Validate VCI value
	 */
	if (argc) {
		vci = strtol(argv[0], &cp, 0);
		if ((*cp != '\0') || (vci <= 0) || (vci >= 1 << 16)) {
			fprintf(stderr, "%s: Invalid VCI value\n",
					prog);
			exit(1);
		}
		argc--; argv++;
	}

	/*
	 * Get VCC information
	 */
	vcc_info_len = get_vcc_info(intf, &vcc_info);
	if (vcc_info_len == 0)
		exit(1);
	else if (vcc_info_len < 0) {
		fprintf(stderr, "%s: ", prog);
		switch (errno) {
		case ENOPROTOOPT:
		case EOPNOTSUPP:
			perror("Internal error");
			break;
		case ENXIO:
			fprintf(stderr, "Not an ATM device\n");
			break;
		default:
			perror("ioctl (AIOCINFO)");
			break;
		}
		exit(1);
	}

	/*
	 * Sort the VCC information
	 */
	qsort((void *) vcc_info,
			vcc_info_len / sizeof(struct air_vcc_rsp),
			sizeof(struct air_vcc_rsp),
			vcc_compare);

	/*
	 * Display the VCC statistics
	 */
	vcc_info_base = vcc_info;
	for (; vcc_info_len >= sizeof(struct air_vcc_rsp);
			vcc_info_len-=sizeof(struct air_vcc_rsp),
			vcc_info++) {
		if (vpi != -1 && vcc_info->avp_vpi != vpi)
			continue;
		if (vci != -1 && vcc_info->avp_vci != vci)
			continue;
		print_vcc_stats(vcc_info);
	}
	free(vcc_info_base);
}


/*
 * Process VCC information command
 * 
 * Command format: 
 *	atm show VCC [<interface-name> [<vpi> [<vci>] | PVC | SVC]] 
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
show_vcc(argc, argv, cmdp)
	int		argc;
	char		**argv;
	struct cmd	*cmdp;
{
	int	vcc_info_len;
	int	vpi = -1, vci = -1, show_pvc = 0, show_svc = 0;
	char	*cp, *intf = NULL;
	struct air_vcc_rsp	*vcc_info, *vcc_info_base;

	/*
	 * Validate interface name
	 */
	if (argc) {
		if (strlen(argv[0]) > IFNAMSIZ - 1) {
			fprintf(stderr, "%s: Illegal interface name\n",
					prog);
			exit(1);
		}
		intf = argv[0];
		argc--; argv++;
	}

	/*
	 * Validate VPI value
	 */
	if (argc) {
		if (strcasecmp(argv[0], "pvc"))
			show_pvc = 1;
		else if (strcasecmp(argv[0], "svc"))
			show_svc = 1;
		else {
			vpi = strtol(argv[0], &cp, 0);
			if ((*cp != '\0') || (vpi < 0) ||
					(vpi >= 1 << 8)) {
				fprintf(stderr, "%s: Invalid VPI value\n", prog);
				exit(1);
			}
		}
		argc--; argv++;
	}

	/*
	 * Validate VCI value
	 */
	if (argc) {
		vci = strtol(argv[0], &cp, 0);
		if ((*cp != '\0') || (vci <= 0) || (vci >= 1 << 16)) {
			fprintf(stderr, "%s: Invalid VCI value\n",
					prog);
			exit(1);
		}
		argc--; argv++;
	}

	/*
	 * Get VCC information
	 */
	vcc_info_len = get_vcc_info(intf, &vcc_info);
	if (vcc_info_len == 0)
		exit(1);
	else if (vcc_info_len < 0) {
		fprintf(stderr, "%s: ", prog);
		switch (errno) {
		case ENOPROTOOPT:
		case EOPNOTSUPP:
			perror("Internal error");
			break;
		case ENXIO:
			fprintf(stderr, "Not an ATM device\n");
			break;
		default:
			perror("ioctl (AIOCINFO)");
			break;
		}
		exit(1);
	}

	/*
	 * Sort the VCC information
	 */
	qsort((void *) vcc_info,
			vcc_info_len/sizeof(struct air_vcc_rsp),
			sizeof(struct air_vcc_rsp),
			vcc_compare);

	/*
	 * Display the VCC information
	 */
	vcc_info_base = vcc_info;
	for (; vcc_info_len >= sizeof(struct air_vcc_rsp);
			vcc_info_len-=sizeof(struct air_vcc_rsp),
			vcc_info++) {
		if (vpi != -1 && vcc_info->avp_vpi != vpi)
			continue;
		if (vci != -1 && vcc_info->avp_vci != vci)
			continue;
		if (show_pvc && vcc_info->avp_type & VCC_PVC)
			continue;
		if (show_svc && vcc_info->avp_type & VCC_SVC)
			continue;
		print_vcc_info(vcc_info);
	}
	free(vcc_info_base);
}


/*
 * Process version command
 * 
 * Command format: 
 *	atm show version
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
show_version(argc, argv, cmdp)
	int		argc;
	char		**argv;
	struct cmd	*cmdp;
{
	int	buf_len = sizeof(struct air_version_rsp);
	struct atminfreq	air;
	struct air_version_rsp	*ver_info, *ver_info_base;

	/*
	 * Get network interface information from the kernel
	 */
	air.air_opcode = AIOCS_INF_VER;
	buf_len = do_info_ioctl(&air, buf_len);
	if (buf_len < 0) {
		fprintf(stderr, "%s: ", prog);
		switch (errno) {
		case ENOPROTOOPT:
		case EOPNOTSUPP:
			perror("Internal error");
			break;
		case ENXIO:
			fprintf(stderr, "Not an ATM device\n");
			break;
		default:
			perror("ioctl (AIOCINFO)");
			break;
		}
		exit(1);
	}

	/*
	 * Print the network interface information
	 */
	ver_info_base = ver_info =
			(struct air_version_rsp *) air.air_buf_addr;
	for (; buf_len >= sizeof(struct air_version_rsp); ver_info++,
			buf_len -= sizeof(struct air_version_rsp)) {
		print_version_info(ver_info);
	}
	free(ver_info_base);
}


/*
 * Comparison function for qsort
 * 
 * Arguments:
 *	p1	pointer to the first VCC response
 *	p2	pointer to the second VCC response
 *
 * Returns:
 *	int	a number less than, greater than, or equal to zero,
 *		depending on whether *p1 is less than, greater than, or
 *		equal to *p2
 *
 */
static int
vcc_compare(p1, p2)
	const void *p1, *p2;
{
	int rc;
	struct air_vcc_rsp	*c1, *c2;

	c1 = (struct air_vcc_rsp *) p1;
	c2 = (struct air_vcc_rsp *) p2;

	/*
	 * Compare the interface names
	 */
	rc = strcmp(c1->avp_intf, c2->avp_intf);
	if (rc)
		return(rc);

	/*
	 * Compare the VPI values
	 */
	rc = c1->avp_vpi - c2->avp_vpi;
	if (rc)
		return(rc);

	/*
	 * Compare the VCI values
	 */
	rc = c1->avp_vci - c2->avp_vci;
	if (rc)
		return(rc);

	/*
	 * Compare the types
	 */
	rc = c1->avp_type - c2->avp_type;
	return(rc);
}


/*
 * Comparison function for qsort
 * 
 * Arguments:
 *	p1	pointer to the first VCC response
 *	p2	pointer to the second VCC response
 *
 * Returns:
 *	int	a number less than, greater than, or equal to zero,
 *		depending on whether *p1 is less than, greater than, or
 *		equal to *p2
 *
 */
static int
ip_vcc_compare(p1, p2)
	const void *p1, *p2;
{
	int rc;
	struct air_ip_vcc_rsp	*c1, *c2;

	c1 = (struct air_ip_vcc_rsp *) p1;
	c2 = (struct air_ip_vcc_rsp *) p2;

	/*
	 * Compare the interface names
	 */
	rc = strcmp(c1->aip_intf, c2->aip_intf);
	if (rc)
		return(rc);

	/*
	 * Compare the VPI values
	 */
	rc = c1->aip_vpi - c2->aip_vpi;
	if (rc)
		return(rc);

	/*
	 * Compare the VCI values
	 */
	rc = c1->aip_vci - c2->aip_vci;
	return(rc);
}


/*
 * Comparison function for qsort
 * 
 * Arguments:
 *	p1	pointer to the first ARP or IP map entry
 *	p2	pointer to the second ARP or IP map entry
 *
 * Returns:
 *	int	a number less than, greater than, or equal to zero,
 *		depending on whether *p1 is less than, greater than, or
 *		equal to *p2
 *
 */
static int
arp_compare(p1, p2)
	const void *p1, *p2;
{
	int rc;
	struct air_arp_rsp	*c1, *c2;
	struct sockaddr_in	*sin1, *sin2;

	c1 = (struct air_arp_rsp *) p1;
	c2 = (struct air_arp_rsp *) p2;
	sin1 = (struct sockaddr_in *) &c1->aap_arp_addr;
	sin2 = (struct sockaddr_in *) &c2->aap_arp_addr;

	/*
	 * Compare the IP addresses
	 */
	if ((rc = sin1->sin_family - sin2->sin_family) != 0)
		return(rc);
	if ((rc = sin1->sin_addr.s_addr - sin2->sin_addr.s_addr) != 0)
		return(rc);

	/*
	 * Compare the ATM addresses
	 */
	if ((rc = c1->aap_addr.address_format - c2->aap_addr.address_format) != 0)
		return(rc);
	if ((rc = c1->aap_addr.address_length - c2->aap_addr.address_length) != 0)
		return(rc);
	switch(c1->aap_addr.address_format) {
	case T_ATM_ABSENT:
		rc = 0;
		break;
	case T_ATM_ENDSYS_ADDR:
		rc = bcmp((caddr_t)c1->aap_addr.address,
				(caddr_t)c2->aap_addr.address,
				sizeof(Atm_addr_nsap));
		break;
	case T_ATM_E164_ADDR:
		rc = bcmp((caddr_t)c1->aap_addr.address,
				(caddr_t)c2->aap_addr.address,
				sizeof(Atm_addr_e164));
		break;
	case T_ATM_SPANS_ADDR:
		rc = bcmp((caddr_t)c1->aap_addr.address,
				(caddr_t)c2->aap_addr.address,
				sizeof(Atm_addr_spans));
		break;
	}

	return(rc);
}
