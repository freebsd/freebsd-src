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
 * Main routine
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
#include <netatm/atm_cm.h>
#include <netatm/atm_sigmgr.h>
#include <netatm/atm_ioctl.h>

#include <errno.h>
#include <libatm.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <err.h>

#include "atm.h"

#ifndef lint
__RCSID("@(#) $FreeBSD$");
#endif


/*
 * Usage string
 */
#define USAGE_STR	"Interface management subcommands:\n\
    attach <intf> <protocol>\n\
    detach <intf>\n\
    set mac <intf> <MAC/ESI address>\n\
    set netif <intf> <prefix> <n>\n\
    set prefix <intf> <NSAP prefix>\n\
    show config [<intf>]\n\
    show interface [<intf>]\n\
    show netif [<netif>]\n\
    show stats interface [<intf> phy | dev | atm | aal0 | aal4 |\n\
        aal5 | driver]\n\
\n\
VC management subcommands:\n\
    add pvc <intf> <vpi> <vci> <aal> <encaps> <owner> ...\n\
    delete pvc <intf> <vpi> <vci>\n\
    delete svc <intf> <vpi> <vci>\n\
    show stats vcc [<intf> [vpi [vci]]]\n\
    show vcc [<intf> [<vpi> [<vci>] | SVC | PVC]]\n\
\n\
IP management subcommands:\n\
    add arp [<netif>] <IP addr> <ATM addr>\n\
    add pvc <intf> <vpi> <vci> <aal> <encaps> IP <netif> <IP addr> |\n\
        dynamic\n\
    delete arp [<netif>] <IP addr>\n\
    set arpserver <netif> <server> <IP prefix> ...\n\
    show arp [<host>]\n\
    show arpserver [<netif>]\n\
    show ipvcc [<IP addr> | <netif>]\n\
\n\
Miscellaneous subcommands:\n\
    help\n\
    show version\n"


/*
 * Local definitions
 */

struct cmd	add_subcmd[];
struct cmd	dlt_subcmd[];
struct cmd	set_subcmd[];
struct cmd	show_subcmd[];
struct cmd	stats_subcmd[];

struct cmd	cmds[] = {
	{ "add",	0,	0,	NULL,	(char *) add_subcmd },
	{ "attach",	2,	2,	attach,	"<intf> <protocol>" },
	{ "delete",	0,	0,	NULL,	(char *) dlt_subcmd },
	{ "detach",	1,	1,	detach,	"<intf>" },
	{ "set",	0,	0,	NULL,	(char *) set_subcmd },
	{ "show",	0,	0,	NULL,	(char *) show_subcmd },
	{ "help",	0,	99,	help,	"" },
	{ 0,		0,	0,	NULL,	"" }
};

struct cmd add_subcmd[] = {
	{ "arp",	2,	3,	arp_add, "[<netif>] <IP addr> <ATM addr>" },
	{ "pvc",	6,	16,	pvc_add, "<intf> <vpi> <vci> <aal> <encaps> <owner> <netif> ... [UBR | CBR | VBR]" },
	{ 0,		0,	0,	NULL,	"" }
};

struct cmd dlt_subcmd[] = {
	{ "arp",	1,	2,	arp_dlt, "[<netif>] <IP addr>" },
	{ "pvc",	3,	3,	pvc_dlt, "<intf> <vpi> <vci>" },
	{ "svc",	3,	3,	svc_dlt, "<intf> <vpi> <vci>" },
	{ 0,		0,	0,	NULL,	"" }
};

struct cmd set_subcmd[] = {
	{ "arpserver",	2,	18,	set_arpserver, "<netif> <server>" },
	{ "mac",	2,	2,	set_macaddr, "<intf> <MAC/ESI address>" },
	{ "netif",	3,	3,	set_netif, "<intf> <prefix> <n>" },
	{ "prefix",	2,	2,	set_prefix, "<intf> <NSAP prefix>" },
	{ 0,		0,	0,	NULL,	""}
};

struct cmd show_subcmd[] = {
	{ "arp",	0,	1,	show_arp, "[<host>]" },
	{ "arpserver",	0,	1,	show_arpserv, "[<netif>]" },
	{ "config",	0,	1,	show_config, "[<intf>]" },
	{ "interface",	0,	1,	show_intf, "[<intf>]" },
	{ "ipvcc",	0,	3,	show_ip_vcc, "[<IP addr> | <netif>]" },
	{ "netif",	0,	1,	show_netif, "[<netif>]" },
	{ "stats",	0,	3,	NULL, (char *) stats_subcmd },
	{ "vcc",	0,	3,	show_vcc, "[<intf>] [<vpi> [<vci>] | SVC | PVC]" },
	{ "version",	0,	0,	show_version, "" },
	{ 0,		0,	0,	NULL,	"" }
};

struct cmd stats_subcmd[] = {
	{ "interface",	0,	2,	show_intf_stats, "[<intf> [cfg | phy | dev | atm | aal0 | aal4 | aal5 | driver]]" },
	{ "vcc",	0,	3,	show_vcc_stats, "[<intf> [vpi [vci]]]" },
	{ 0,		0,	0,	NULL,	"" }
};


/*
 * Supported signalling protocols
 */
struct proto	protos[] = {
	{ "SIGPVC",	ATM_SIG_PVC },
	{ "SPANS",	ATM_SIG_SPANS },
	{ "UNI30",	ATM_SIG_UNI30 },
	{ "UNI31",	ATM_SIG_UNI31 },
	{ "UNI40",	ATM_SIG_UNI40 },
	{ 0,		0 }
};

/*
 * Supported VCC owners
 */
struct owner	owners[] = {
	{ "IP",		ENDPT_IP,	ip_pvcadd },
	{ "SPANS",	ENDPT_SPANS_SIG,0 },
	{ "SPANS CLS",	ENDPT_SPANS_CLS,0 },
	{ "UNI SIG",	ENDPT_UNI_SIG,	0 },
	{ 0,		0,		0 }
};

/*
 * Supported AAL parameters
 */
struct aal	aals[] = {
	{ "Null",	ATM_AAL0 },
	{ "AAL0",	ATM_AAL0 },
	{ "AAL1",	ATM_AAL1 },
	{ "AAL2",	ATM_AAL2 },
	{ "AAL4",	ATM_AAL3_4 },
	{ "AAL3",	ATM_AAL3_4 },
	{ "AAL3/4",	ATM_AAL3_4 },
	{ "AAL5",	ATM_AAL5 },
	{ 0,	0 },
};

/*
 * Supported VCC encapsulations
 */
struct encaps	encaps[] = {
	{ "Null",	ATM_ENC_NULL },
	{ "None",	ATM_ENC_NULL },
	{ "LLC/SNAP",	ATM_ENC_LLC },
	{ "LLC",	ATM_ENC_LLC },
	{ "SNAP",	ATM_ENC_LLC },
	{ 0,	0 },
};


/*
 * Supported ATM traffic types
 */
struct traffics traffics[] = {
	{ "UBR",	T_ATM_UBR,	1,	"UBR <pcr>" },
	{ "CBR",	T_ATM_CBR,	1,	"CBR <pcr>" },
	{ "VBR",	T_ATM_VBR,	3,	"VBR <pcr> <scr> <mbs>" },
#ifdef notyet
	{ "ABR",	T_ATM_ABR,	2,	"ABR <arg1> <arg2>" },
#endif
	{ NULL, 0, 0, NULL }
};

char	*prog;
char	prefix[128] = "";


int
main(argc, argv)
	int	argc;
	char	**argv;
{
	int	error;

	/*
	 * Save program name, ignoring any path components
	 */
	if ((prog = (char *)strrchr(argv[0], '/')) != NULL)
		prog++;
	else
		prog = argv[0];

	if (argc < 2) {
		usage(cmds, "");
		exit(1);
	}
	argc--; argv++;
		
	/*
	 * Validate and process command
	 */
	if ((error = do_cmd(cmds, argc, argv)) != 0)
		usage(cmds, "");

	exit(error);
}


/*
 * Validate and process user command
 * 
 * Arguments:
 *	descp	pointer to command description array
 *	argc	number of arguments left in command
 *	argv	pointer to argument strings
 *
 * Returns:
 *	none
 *
 */
int
do_cmd(descp, argc, argv)
	struct cmd	*descp;
	int		argc;
	char		**argv;
{
	struct cmd	*cmdp = 0;

	/*
	 * Make sure we have paramaters to process
	 */
	if (!argc) {
		usage(cmds, "");
		exit(1);
	}

	/*
	 * Figure out what command user wants
	 */
	for (; descp->name; descp++) {
		/*
		 * Use an exact match if there is one
		 */
		if (!strcasecmp(descp->name, argv[0])) {
			cmdp = descp;
			break;
		}
		/*
		 * Look for a match on the first part of keyword
		 */
		if (!strncasecmp(descp->name, argv[0], strlen(argv[0]))) {
			if (cmdp) {
				fprintf(stderr, "%s: Ambiguous parameter \"%s\"\n",
						prog, argv[0]);
				exit(1);
			}
			cmdp = descp;
		}
	}
	if (!cmdp)
		return(1);
	argc--; argv++;

	/*
	 * See if this command has subcommands
	 */
	if (cmdp->func == NULL) {
		strcat(prefix, cmdp->name);
		strcat(prefix, " ");
		return(do_cmd((struct cmd *)cmdp->help, argc, argv));
	}

	/*
	 * Minimal validation
	 */
	if ((argc < cmdp->minp) || (argc > cmdp->maxp)) {
		fprintf(stderr, "%s: Invalid number of arguments\n",
			prog);
		fprintf(stderr, "\tformat is: %s%s %s\n",
			prefix, cmdp->name, cmdp->help);
		exit(1);
	}

	/*
	 * Process command
	 */
	(*cmdp->func)(argc, argv, cmdp);
	return(0);
}


/*
 * Print command usage information
 * 
 * Arguments:
 *	cmdp	pointer to command description 
 * 	pref	pointer current command prefix 
 *
 * Returns:
 *	none
 *
 */
void
usage(cmdp, pref)
	struct cmd	*cmdp;
	char		*pref;
{
	fprintf(stderr, "usage: %s command [arg] [arg]...\n", prog);
	fprintf(stderr, USAGE_STR);
}


/*
 * Process interface attach command
 * 
 * Command format: 
 *	atm attach <interface_name> <protocol_name>
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
attach(argc, argv, cmdp)
	int		argc;
	char		**argv;
	struct cmd	*cmdp;
{
	struct atmcfgreq	aar;
	struct proto	*prp;
	int		s;

	/*
	 * Validate interface name
	 */
	if (strlen(argv[0]) > sizeof(aar.acr_att_intf) - 1) {
		fprintf(stderr, "%s: Illegal interface name\n", prog);
		exit(1);
	}

	/*
	 * Find/validate requested signalling protocol
	 */
	for (prp = protos; prp->p_name; prp++) {
		if (strcasecmp(prp->p_name, argv[1]) == 0)
			break;
	}
	if (prp->p_name == NULL) {
		fprintf(stderr, "%s: Unknown signalling protocol\n", prog);
		exit(1);
	}


	/*
	 * Build ioctl request
	 */
	aar.acr_opcode = AIOCS_CFG_ATT;
	strncpy(aar.acr_att_intf, argv[0], sizeof(aar.acr_att_intf));
	aar.acr_att_proto = prp->p_id;

	/*
	 * Tell the kernel to do the attach
	 */
	s = socket(AF_ATM, SOCK_DGRAM, 0);
	if (s < 0) {
		sock_error(errno);
	}
	if (ioctl(s, AIOCCFG, (caddr_t)&aar) < 0) {
		fprintf(stderr, "%s: ", prog);
		switch (errno) {
		case EINVAL:
		case EOPNOTSUPP:
		case EPROTONOSUPPORT:
			perror("Internal error");
			break;
		case ENOMEM:
			fprintf(stderr, "Kernel memory exhausted\n");
			break;
		case EEXIST:
			fprintf(stderr, "Signalling manager already attached to %s\n",
					argv[0]);
			break;
		case ENETDOWN:
			fprintf(stderr, "ATM network is inoperable\n");
			break;
		case EPERM:
			fprintf(stderr, "Must be super user to use attach subcommand\n");
			break;
		case ENXIO:
			fprintf(stderr, "%s is not an ATM device\n",
					argv[0]);
			break;
		case ETOOMANYREFS:
			fprintf(stderr, "%s has too few or too many network interfaces\n",
					argv[0]);
			break;
		default:
			perror("Ioctl (AIOCCFG) attach");
			break;
		}
		exit(1);
	}
	(void)close(s);
}


/*
 * Process interface detach command
 * 
 * Command format: 
 *	atm detach <interface_name>
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
detach(argc, argv, cmdp)
	int		argc;
	char		**argv;
	struct cmd	*cmdp;
{
	struct atmcfgreq	adr;
	int		s;

	/*
	 * Validate interface name
	 */
	if (strlen(argv[0]) > sizeof(adr.acr_det_intf) - 1) {
		fprintf(stderr, "%s: Illegal interface name\n", prog);
		exit(1);
	}

	/*
	 * Build ioctl request
	 */
	adr.acr_opcode = AIOCS_CFG_DET;
	strncpy(adr.acr_det_intf, argv[0], sizeof(adr.acr_det_intf));

	/*
	 * Tell the kernel to do the detach
	 */
	s = socket(AF_ATM, SOCK_DGRAM, 0);
	if (s < 0) {
		sock_error(errno);
	}
	if (ioctl(s, AIOCCFG, (caddr_t)&adr) < 0) {
		fprintf(stderr, "%s: ", prog);
		switch (errno) {
		case EALREADY:
			fprintf(stderr, "Signalling manager already detaching from %s\n",
					argv[0]);
			break;
		case EINVAL:
			perror("Internal error");
			break;
		case EPERM:
			fprintf(stderr, "Must be super user to use detach subcommand\n");
			break;
		default:
			perror("ioctl (AIOCCFG) detach");
			break;
		}
		exit(1);
	}
	(void)close(s);
}


/*
 * Process PVC add command
 * 
 * Command format: 
 *	atm add PVC <interface_name> <vpi> <vci> <aal> <encaps>
 *		<owner_name> [ubr <PCR> | cbr <PCR> | vbr <PCR> <SCR> <MBS>]
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
pvc_add(argc, argv, cmdp)
	int		argc;
	char		**argv;
	struct cmd	*cmdp;
{
	struct atmaddreq	apr;
	struct atminfreq	air;
	struct air_int_rsp	*int_info;
	struct owner	*owp;
	struct aal	*alp;
	struct encaps	*enp;
 	const struct traffics	*trafp;
	char	*cp;
	u_long	v;
	int	buf_len, s;

	/*
	 * Initialize opcode and flags
	 */
	apr.aar_opcode = AIOCS_ADD_PVC;
	apr.aar_pvc_flags = 0;

	/*
	 * Validate interface name and issue an information
	 * request IOCTL for the interface
	 */
	if (strlen(argv[0]) > sizeof(apr.aar_pvc_intf) - 1) {
		fprintf(stderr, "%s: Illegal interface name\n", prog);
		exit(1);
	}
	UM_ZERO(air.air_int_intf, sizeof(air.air_int_intf));
	strcpy(air.air_int_intf, argv[0]);
	buf_len = sizeof(struct air_int_rsp);
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
	int_info = (struct air_int_rsp *) air.air_buf_addr;
	strcpy(apr.aar_pvc_intf, argv[0]);
	argc--; argv++;

	/*
	 * Validate vpi/vci values
	 */
	errno = 0;
	v = strtoul(argv[0], &cp, 0);
	if (errno != 0 || *cp != '\0' || v >= 1 << 8)
		errx(1, "Invalid VPI value '%s'", argv[0]);
	apr.aar_pvc_vpi = (u_short)v;
	argc--;
	argv++;

	errno = 0;
	v = strtoul(argv[0], &cp, 0);
	if (errno != 0 || *cp != '\0' || v < MIN_VCI || v >= 1 << 16)
		errx(1, "Invalid VCI value '%s'", argv[0]);
	apr.aar_pvc_vci = (u_short)v;
	argc--;
	argv++;

	/*
	 * Validate requested PVC AAL
	 */
	for (alp = aals; alp->a_name; alp++) {
		if (strcasecmp(alp->a_name, argv[0]) == 0)
			break;
	}
	if (alp->a_name == NULL)
		errx(1, "Invalid PVC AAL '%s'", argv[0]);
	apr.aar_pvc_aal = alp->a_id;
	argc--;
	argv++;

	/*
	 * Validate requested PVC encapsulation
	 */
	for (enp = encaps; enp->e_name; enp++) {
		if (strcasecmp(enp->e_name, argv[0]) == 0)
			break;
	}
	if (enp->e_name == NULL)
		errx(1, "Invalid PVC encapsulation '%s'", argv[0]);
	apr.aar_pvc_encaps = enp->e_id;
	argc--;
	argv++;

	/*
	 * Validate requested PVC owner
	 */
	for (owp = owners; owp->o_name; owp++) {
		if (strcasecmp(owp->o_name, argv[0]) == 0)
			break;
	}
	if (owp->o_name == NULL)
		errx(1, "Unknown PVC owner '%s'", argv[0]);
	apr.aar_pvc_sap = owp->o_sap;
	if (owp->o_pvcadd == NULL)
		errx(1, "Unsupported PVC owner '%s'", argv[0]);
	argc--;
	argv++;

	/*
	 * Perform service user processing
	 */
	(*owp->o_pvcadd)(argc, argv, cmdp, &apr, int_info);

	argc -= 2;
	argv += 2;

	if (argc > 0) {
		/*
		 * Validate requested traffic
		 */
		for (trafp = traffics; trafp->t_name; trafp++) {
			if (strcasecmp(trafp->t_name, argv[0]) == 0)
				break;
		}
		if (trafp->t_name == NULL)
			errx(1, "Unknown traffic type '%s'", argv[0]);
		apr.aar_pvc_traffic_type = trafp->t_type;
		argc--;
		argv++;

		if (trafp->t_argc != argc)
			errx(1, "Invalid traffic parameters\n\t %s",
			    trafp->help);
		switch (trafp->t_type) {

		  case T_ATM_UBR:
		  case T_ATM_CBR:
			errno = 0;
			v = strtoul(argv[0], &cp, 0);
			if (errno != 0 || *cp != '\0' || v >= 1 << 24)
				errx(1, "Invalid PCR value '%s'", argv[0]);
			apr.aar_pvc_traffic.forward.PCR_high_priority = (int32_t) v;
			apr.aar_pvc_traffic.forward.PCR_all_traffic = (int32_t) v;
			apr.aar_pvc_traffic.backward.PCR_high_priority = (int32_t) v;
			apr.aar_pvc_traffic.backward.PCR_all_traffic = (int32_t) v;
			argc--;
			argv++;
			apr.aar_pvc_traffic.forward.SCR_high_priority = T_ATM_ABSENT;
			apr.aar_pvc_traffic.forward.SCR_all_traffic = T_ATM_ABSENT;
			apr.aar_pvc_traffic.backward.SCR_high_priority = T_ATM_ABSENT;
			apr.aar_pvc_traffic.backward.SCR_all_traffic = T_ATM_ABSENT;
			apr.aar_pvc_traffic.forward.MBS_high_priority = T_ATM_ABSENT;
			apr.aar_pvc_traffic.forward.MBS_all_traffic = T_ATM_ABSENT;
			apr.aar_pvc_traffic.backward.MBS_high_priority = T_ATM_ABSENT;
			apr.aar_pvc_traffic.backward.MBS_all_traffic = T_ATM_ABSENT;
			break;

		case T_ATM_VBR: /* VBR pcr scr mbs */
			errno = 0;
			v = strtoul(argv[0], &cp, 0);
			if (errno != 0 || *cp != '\0' || v >= 1 << 24)
				errx(1, "Invalid PCR value '%s'", argv[0]);
			apr.aar_pvc_traffic.forward.PCR_high_priority = (int32_t)v;
			apr.aar_pvc_traffic.forward.PCR_all_traffic = (int32_t)v;
			apr.aar_pvc_traffic.backward.PCR_high_priority = (int32_t)v;
			apr.aar_pvc_traffic.backward.PCR_all_traffic = (int32_t)v;
			argc--;
			argv++;

			errno = 0;
			v = strtoul(argv[0], &cp, 0);
			if (errno != 0 || *cp != '\0' || v >= 1 << 24)
				errx(1, "Invalid SCR value '%s'", argv[0]);
			apr.aar_pvc_traffic.forward.SCR_high_priority = (int32_t)v;
			apr.aar_pvc_traffic.forward.SCR_all_traffic = (int32_t)v;
			apr.aar_pvc_traffic.backward.SCR_high_priority = (int32_t)v;
			apr.aar_pvc_traffic.backward.SCR_all_traffic = (int32_t)v;
			argc--;
			argv++;

			errno = 0;
			v = strtol(argv[0], &cp, 0);
			if (errno != 0 || *cp != '\0' || v >= 1 << 24)
				errx(1, "Invalid MBS value '%s'", argv[0]);
			apr.aar_pvc_traffic.forward.MBS_high_priority = (int32_t)v;
			apr.aar_pvc_traffic.forward.MBS_all_traffic = (int32_t)v;
			apr.aar_pvc_traffic.backward.MBS_high_priority = (int32_t)v;
			apr.aar_pvc_traffic.backward.MBS_all_traffic = (int32_t)v;
			argc--;
			argv++;

			break;

		case T_ATM_ABR:
			errx(1, "ABR not yet supported");

		default:
			errx(1, "Unsupported traffic type '%d'", trafp->t_type);
		}
	} else {
		/*
		 * No PVC traffic type
		 */
		apr.aar_pvc_traffic_type = T_ATM_NULL;
	}
	if (argc > 0)
		errx(1, "Too many parameters");

	/*
	 * Tell the kernel to add the PVC
	 */
	s = socket(AF_ATM, SOCK_DGRAM, 0);
	if (s < 0) {
		sock_error(errno);
	}
	if (ioctl(s, AIOCADD, (caddr_t)&apr) < 0) {
		fprintf(stderr, "%s: ", prog);
		switch (errno) {
		case EPROTONOSUPPORT:
		case ENOPROTOOPT:
			perror("Internal error");
			break;
		case EINVAL:
			fprintf(stderr, "Invalid parameter\n");
			break;
		case EEXIST:
			fprintf(stderr, "PVC already exists\n");
			break;
		case ENETDOWN:
			fprintf(stderr, "ATM network is inoperable\n");
			break;
		case ENOMEM:
			fprintf(stderr, "Kernel memory exhausted\n");
			break;
		case EPERM:
			fprintf(stderr, "Must be super user to use add subcommand\n");
			break;
		case ERANGE:
			fprintf(stderr, "Invalid VPI or VCI value\n");
			break;
		default:
			perror("ioctl (AIOCADD) add PVC");
			break;
		}
		exit(1);
	}
	(void)close(s);
}


/*
 * Process ARP add command
 * 
 * Command formats: 
 *	atm add arp [<netif>] <IP addr> <ATM addr>
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
arp_add(argc, argv, cmdp)
	int		argc;
	char		**argv;
	struct cmd	*cmdp;
{
	int			len, s;
	struct atmaddreq	apr;
	Atm_addr		host_atm;
	struct sockaddr_in	*sin;
	union {
		struct sockaddr_in	sin;
		struct sockaddr		sa;
	} host_ip;

	/*
	 * Initialize add request structure
	 */
	UM_ZERO(&apr, sizeof(apr));

	/*
	 * Get network interface name if one is present
	 */
	if (argc == 3) {
		check_netif_name(argv[0]);
		strcpy(apr.aar_arp_intf, argv[0]);
		argc--; argv++;
	}

        /*
         * Get IP address of specified host name
         */
	UM_ZERO(&host_ip, sizeof(host_ip));
	host_ip.sa.sa_family = AF_INET;
	sin = get_ip_addr(argv[0]);
	host_ip.sin.sin_addr.s_addr = sin->sin_addr.s_addr;
	argc--; argv++;

	/*
	 * Get specified ATM address
	 */
	len = get_hex_atm_addr(argv[0], (u_char *)host_atm.address,
			sizeof(Atm_addr_nsap));
	switch(len) {
	case sizeof(Atm_addr_nsap):
		host_atm.address_format = T_ATM_ENDSYS_ADDR;
		host_atm.address_length = sizeof(Atm_addr_nsap);
		break;
	case sizeof(Atm_addr_spans):
		host_atm.address_format = T_ATM_SPANS_ADDR;
		host_atm.address_length = sizeof(Atm_addr_spans);
		break;
	default:
		fprintf(stderr, "%s: Invalid ATM address\n", prog);
		exit(1);
	}

	/*
	 * Build IOCTL request
	 */
	apr.aar_opcode = AIOCS_ADD_ARP;
	apr.aar_arp_dst = host_ip.sa;
	ATM_ADDR_COPY(&host_atm, &apr.aar_arp_addr);
	apr.aar_arp_origin = ARP_ORIG_PERM;

	/*
	 * Tell the kernel to add the ARP table entry
	 */
	s = socket(AF_ATM, SOCK_DGRAM, 0);
	if (s < 0) {
		sock_error(errno);
	}
	if (ioctl(s, AIOCADD, (caddr_t)&apr) < 0) {
		fprintf(stderr, "%s: ", prog);
		switch (errno) {
		case EINVAL:
			fprintf(stderr, "Invalid parameter\n");
			break;
		case EPERM:
			fprintf(stderr, "Must be super user to use add subcommand\n");
			break;
		case EADDRNOTAVAIL:
			fprintf(stderr, "IP address not valid for interface\n");
			break;
		default:
			perror("ioctl (AIOCADD) add");
			break;
		}
		exit(1);
	}
	(void)close(s);
}


/*
 * Process PVC delete command
 * 
 * Command formats: 
 *	atm delete pvc <interface_name> <vpi> <vci>
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
pvc_dlt(argc, argv, cmdp)
	int		argc;
	char		**argv;
	struct cmd	*cmdp;
{
	struct atmdelreq	apr;

	/*
	 * Set opcode
	 */
	apr.adr_opcode = AIOCS_DEL_PVC;

	/*
	 * Complete request by calling subroutine
	 */
	vcc_dlt(argc, argv, cmdp, &apr);
}


/*
 * Process SVC delete command
 * 
 * Command formats: 
 *	atm delete svc <interface_name> <vpi> <vci>
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
svc_dlt(argc, argv, cmdp)
	int		argc;
	char		**argv;
	struct cmd	*cmdp;
{
	struct atmdelreq	apr;

	/*
	 * Set opcode
	 */
	apr.adr_opcode = AIOCS_DEL_SVC;

	/*
	 * Complete request by calling subroutine
	 */
	vcc_dlt(argc, argv, cmdp, &apr);
}


/*
 * Complete an SVC or PVC delete command
 * 
 * Arguments:
 *	argc	number of arguments to command
 *	argv	pointer to argument strings
 *	cmdp	pointer to command description 
 *	apr	pointer to ATM delete IOCTL structure
 *
 * Returns:
 *	none
 *
 */
void
vcc_dlt(argc, argv, cmdp, apr)
	int			argc;
	char			**argv;
	struct cmd		*cmdp;
	struct atmdelreq	*apr;
{
	char	*cp;
	long	v;
	int	s;

	/*
	 * Validate interface name
	 */
	if (strlen(argv[0]) > sizeof(apr->adr_pvc_intf) - 1) {
		fprintf(stderr, "%s: Illegal interface name\n", prog);
		exit(1);
	}
	strcpy(apr->adr_pvc_intf, argv[0]);
	argc--; argv++;

	/*
	 * Validate vpi/vci values
	 */
	v = strtol(argv[0], &cp, 0);
	if ((*cp != '\0') || (v < 0) || (v >= 1 << 8)) {
		fprintf(stderr, "%s: Invalid VPI value\n", prog);
		exit(1);
	}
	apr->adr_pvc_vpi = (u_short) v;
	argc--; argv++;

	v = strtol(argv[0], &cp, 0);
	if ((*cp != '\0') || (v < MIN_VCI) || (v >= 1 << 16)) {
		fprintf(stderr, "%s: Invalid VCI value\n", prog);
		exit(1);
	}
	apr->adr_pvc_vci = (u_short) v;
	argc--; argv++;

	/*
	 * Tell the kernel to delete the VCC
	 */
	s = socket(AF_ATM, SOCK_DGRAM, 0);
	if (s < 0) {
		sock_error(errno);
	}
	if (ioctl(s, AIOCDEL, (caddr_t)apr) < 0) {
		fprintf(stderr, "%s: ", prog);
		switch (errno) {
		case EINVAL:
			fprintf(stderr, "Invalid parameter\n");
			break;
		case ENOENT:
			fprintf(stderr, "VCC not found\n");
			break;
		case EALREADY:
			fprintf(stderr, "VCC already being closed\n");
			break;
		case ENXIO:
			fprintf(stderr, "%s is not an ATM device\n",
					apr->adr_pvc_intf);
			break;
		case EPERM:
			fprintf(stderr, "Must be super user to use delete subcommand\n");
			break;
		default:
			perror("ioctl (AIOCDEL) delete");
			break;
		}
		exit(1);
	}
	(void)close(s);
}


/*
 * Process ARP delete command
 * 
 * Command formats: 
 *	atm delete arp <IP addr>
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
arp_dlt(argc, argv, cmdp)
	int		argc;
	char		**argv;
	struct cmd	*cmdp;
{
	int	s;
	struct atmdelreq	apr;
	struct sockaddr_in	*sin;
	union {
		struct sockaddr_in	sin;
		struct sockaddr		sa;
	} host_addr;

	/*
	 * Set opcode
	 */
	UM_ZERO(&apr, sizeof(apr));
	apr.adr_opcode = AIOCS_DEL_ARP;

	/*
	 * Get network interface name if one is present
	 */
	if (argc == 2) {
		check_netif_name(argv[0]);
		strcpy(apr.adr_arp_intf, argv[0]);
		argc--; argv++;
	}

        /*
         * Get IP address of specified host name
         */
	UM_ZERO(&host_addr, sizeof(host_addr));
	host_addr.sa.sa_family = AF_INET;
	sin = get_ip_addr(argv[0]);
	host_addr.sin.sin_addr.s_addr = sin->sin_addr.s_addr;
	apr.adr_arp_dst = host_addr.sa;

	/*
	 * Tell the kernel to delete the ARP table entry
	 */
	s = socket(AF_ATM, SOCK_DGRAM, 0);
	if (s < 0) {
		sock_error(errno);
	}
	if (ioctl(s, AIOCDEL, (caddr_t)&apr) < 0) {
		fprintf(stderr, "%s: ", prog);
		switch (errno) {
		case EINVAL:
			fprintf(stderr, "Invalid parameter\n");
			break;
		case EPERM:
			fprintf(stderr, "Must be super user to use delete subcommand\n");
			break;
		default:
			perror("ioctl (AIOCDEL) delete");
			break;
		}
		exit(1);
	}
	(void)close(s);
}


/*
 * Process help command
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
help(argc, argv, cmdp)
	int		argc;
	char		**argv;
	struct cmd	*cmdp;
{
	usage(cmds, "");
}
