/*
 * natd - Network Address Translation Daemon for FreeBSD.
 *
 * This software ois provided free of charge, with no 
 * warranty of any kind, either expressed or implied.
 * Use at your own risk.
 * 
 * You may copy, modify and distribute this software (natd.c) freely.
 *
 * Ari Suutari <suutari@iki.fi>
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <errno.h>
#include <signal.h>

#include <netdb.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <machine/in_cksum.h>
#include <netinet/tcp.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <net/route.h>
#include <arpa/inet.h>

#include <syslog.h>
#include <alias.h>

#include "natd.h"

/* 
 * Default values for input and output
 * divert socket ports.
 */

#define	DEFAULT_SERVICE	"natd"

/*
 * Function prototypes.
 */

static void DoAliasing (int fd);
static void DaemonMode ();
static void HandleRoutingInfo (int fd);
static void Usage ();
static void PrintPacket (struct ip*);
static void SetAliasAddressFromIfName (char* ifName);
static void InitiateShutdown ();
static void Shutdown ();
static void RefreshAddr ();
static void ParseOption (char* option, char* parms, int cmdLine);
static void ReadConfigFile (char* fileName);
static void SetupPermanentLink (char* parms);
static void SetupPortRedirect (char* parms);
static void SetupAddressRedirect (char* parms);
static void StrToAddr (char* str, struct in_addr* addr);
static int  StrToPort (char* str, char* proto);
static int  StrToProto (char* str);
static int  StrToAddrAndPort (char* str, struct in_addr* addr, char* proto);
static void ParseArgs (int argc, char** argv);
static void FlushPacketBuffer (int fd);

/*
 * Globals.
 */

static	int			verbose;
static 	int			background;
static	int			running;
static	int			assignAliasAddr;
static	char*			ifName;
static  int			ifIndex;
static	int			inPort;
static	int			outPort;
static	int			inOutPort;
static	struct in_addr		aliasAddr;
static 	int			dynamicMode;
static  int			ifMTU;
static	int			aliasOverhead;
static 	int			icmpSock;
static	char			packetBuf[IP_MAXPACKET];
static 	int			packetLen;
static	struct sockaddr_in	packetAddr;
static 	int			packetSock;

int main (int argc, char** argv)
{
	int			divertIn;
	int			divertOut;
	int			divertInOut;
	int			routeSock;
	struct sockaddr_in	addr;
	fd_set			readMask;
	fd_set			writeMask;
	int			fdMax;
/* 
 * Initialize packet aliasing software.
 * Done already here to be able to alter option bits
 * during command line and configuration file processing.
 */
	PacketAliasInit ();
/*
 * Parse options.
 */
	inPort			= 0;
	outPort			= 0;
	verbose 		= 0;
	inOutPort		= 0;
	ifName			= NULL;
	ifMTU			= -1;
	background		= 0;
	running			= 1;
	assignAliasAddr		= 0;
	aliasAddr.s_addr	= INADDR_NONE;
	aliasOverhead		= 12;
	dynamicMode		= 0;
/*
 * Mark packet buffer empty.
 */
	packetSock		= -1;

	ParseArgs (argc, argv);
/*
 * Check that valid aliasing address has been given.
 */
	if (aliasAddr.s_addr == INADDR_NONE && ifName == NULL) {

		fprintf (stderr, "Aliasing address not given.\n");
		exit (1);
	}

	if (aliasAddr.s_addr != INADDR_NONE && ifName != NULL) {

		fprintf (stderr, "Both alias address and interface name "
				 "are not allowed.\n");
		exit (1);
	}
/*
 * Check that valid port number is known.
 */
	if (inPort != 0 || outPort != 0)
		if (inPort == 0 || outPort == 0) {

			fprintf (stderr, "Both input and output ports"
					 " are required.\n");
			exit (1);
		}

	if (inPort == 0 && outPort == 0 && inOutPort == 0)
		ParseOption ("port", DEFAULT_SERVICE, 0);

/*
 * Create divert sockets. Use only one socket if -p was specified
 * on command line. Otherwise, create separate sockets for
 * outgoing and incoming connnections.
 */
	if (inOutPort) {

		divertInOut = socket (PF_INET, SOCK_RAW, IPPROTO_DIVERT);
		if (divertInOut == -1)
			Quit ("Unable to create divert socket.");

		divertIn  = -1;
		divertOut = -1;
/*
 * Bind socket.
 */

		addr.sin_family		= AF_INET;
		addr.sin_addr.s_addr	= INADDR_ANY;
		addr.sin_port		= inOutPort;

		if (bind (divertInOut,
			  (struct sockaddr*) &addr,
			  sizeof addr) == -1)
			Quit ("Unable to bind divert socket.");
	}
	else {

		divertIn = socket (PF_INET, SOCK_RAW, IPPROTO_DIVERT);
		if (divertIn == -1)
			Quit ("Unable to create incoming divert socket.");

		divertOut = socket (PF_INET, SOCK_RAW, IPPROTO_DIVERT);
		if (divertOut == -1)
			Quit ("Unable to create outgoing divert socket.");

		divertInOut = -1;

/*
 * Bind divert sockets.
 */

		addr.sin_family		= AF_INET;
		addr.sin_addr.s_addr	= INADDR_ANY;
		addr.sin_port		= inPort;

		if (bind (divertIn,
			  (struct sockaddr*) &addr,
			  sizeof addr) == -1)
			Quit ("Unable to bind incoming divert socket.");

		addr.sin_family		= AF_INET;
		addr.sin_addr.s_addr	= INADDR_ANY;
		addr.sin_port		= outPort;

		if (bind (divertOut,
			  (struct sockaddr*) &addr,
			  sizeof addr) == -1)
			Quit ("Unable to bind outgoing divert socket.");
	}
/*
 * Create routing socket if interface name specified.
 */
	if (ifName && dynamicMode) {

		routeSock = socket (PF_ROUTE, SOCK_RAW, 0);
		if (routeSock == -1)
			Quit ("Unable to create routing info socket.");
	}
	else
		routeSock = -1;
/*
 * Create socket for sending ICMP messages.
 */
	icmpSock = socket (AF_INET, SOCK_RAW, IPPROTO_ICMP);
	if (icmpSock == -1)
		Quit ("Unable to create ICMP socket.");
/*
 * Become a daemon unless verbose mode was requested.
 */
	if (!verbose)
		DaemonMode ();
/*
 * Catch signals to manage shutdown and
 * refresh of interface address.
 */
	signal (SIGTERM, InitiateShutdown);
	signal (SIGHUP, RefreshAddr);
/*
 * Set alias address if it has been given.
 */
	if (aliasAddr.s_addr != INADDR_NONE)
		PacketAliasSetAddress (aliasAddr);

/*
 * We need largest descriptor number for select.
 */

	fdMax = -1;

	if (divertIn > fdMax)
		fdMax = divertIn;

	if (divertOut > fdMax)
		fdMax = divertOut;

	if (divertInOut > fdMax)
		fdMax = divertInOut;

	if (routeSock > fdMax)
		fdMax = routeSock;

	while (running) {

		if (divertInOut != -1 && !ifName && packetSock == -1) {
/*
 * When using only one socket, just call 
 * DoAliasing repeatedly to process packets.
 */
			DoAliasing (divertInOut);
			continue;
		}
/* 
 * Build read mask from socket descriptors to select.
 */
		FD_ZERO (&readMask);
		FD_ZERO (&writeMask);

/*
 * If there is unsent packet in buffer, use select
 * to check when socket comes writable again.
 */
		if (packetSock != -1) {

			FD_SET (packetSock, &writeMask);
		}
		else {
/*
 * No unsent packet exists - safe to check if
 * new ones are available.
 */
			if (divertIn != -1)
				FD_SET (divertIn, &readMask);

			if (divertOut != -1)
				FD_SET (divertOut, &readMask);

			if (divertInOut != -1)
				FD_SET (divertInOut, &readMask);
		}
/*
 * Routing info is processed always.
 */
		if (routeSock != -1)
			FD_SET (routeSock, &readMask);

		if (select (fdMax + 1,
			    &readMask,
			    &writeMask,
			    NULL,
			    NULL) == -1) {

			if (errno == EINTR)
				continue;

			Quit ("Select failed.");
		}

		if (packetSock != -1)
			if (FD_ISSET (packetSock, &writeMask))
				FlushPacketBuffer (packetSock);

		if (divertIn != -1)
			if (FD_ISSET (divertIn, &readMask))
				DoAliasing (divertIn);

		if (divertOut != -1)
			if (FD_ISSET (divertOut, &readMask))
				DoAliasing (divertOut);

		if (divertInOut != -1) 
			if (FD_ISSET (divertInOut, &readMask))
				DoAliasing (divertInOut);

		if (routeSock != -1)
			if (FD_ISSET (routeSock, &readMask))
				HandleRoutingInfo (routeSock);
	}

	if (background)
		unlink (PIDFILE);

	return 0;
}

static void DaemonMode ()
{
	FILE*	pidFile;

	daemon (0, 0);
	background = 1;

	pidFile = fopen (PIDFILE, "w");
	if (pidFile) {

		fprintf (pidFile, "%d\n", getpid ());
		fclose (pidFile);
	}
}

static void ParseArgs (int argc, char** argv)
{
	int		arg;
	char*		parm;
	char*		opt;
	char		parmBuf[256];

	for (arg = 1; arg < argc; arg++) {

		opt  = argv[arg];
		if (*opt != '-') {

			fprintf (stderr, "Invalid option %s.\n", opt);
			Usage ();
		}

		parm = NULL;
		parmBuf[0] = '\0';

		while (arg < argc - 1) {

			if (argv[arg + 1][0] == '-')
				break;

			if (parm)
				strcat (parmBuf, " ");

			++arg;
			parm = parmBuf;
			strcat (parmBuf, argv[arg]);
		}

		ParseOption (opt + 1, parm, 1);
	}
}

static void DoAliasing (int fd)
{
	int			bytes;
	int			origBytes;
	int			addrSize;
	struct ip*		ip;

	if (assignAliasAddr) {

		SetAliasAddressFromIfName (ifName);
		assignAliasAddr = 0;
	}
/*
 * Get packet from socket.
 */
	addrSize  = sizeof packetAddr;
	origBytes = recvfrom (fd,
			      packetBuf,
			      sizeof packetBuf,
			      0,
			      (struct sockaddr*) &packetAddr,
			      &addrSize);

	if (origBytes == -1) {

		if (errno != EINTR)
			Warn ("Read from divert socket failed.");

		return;
	}
/*
 * This is a IP packet.
 */
	ip = (struct ip*) packetBuf;

	if (verbose) {
		
/*
 * Print packet direction and protocol type.
 */
 
		if (packetAddr.sin_addr.s_addr == INADDR_ANY)
			printf ("Out ");
		else
			printf ("In  ");

		switch (ip->ip_p) {
		case IPPROTO_TCP:
			printf ("[TCP]  ");
			break;

		case IPPROTO_UDP:
			printf ("[UDP]  ");
			break;

		case IPPROTO_ICMP:
			printf ("[ICMP] ");
			break;

		default:
			printf ("[?]    ");
			break;
		}
/*
 * Print addresses.
 */
		PrintPacket (ip);
	}

	if (packetAddr.sin_addr.s_addr == INADDR_ANY) {
/*
 * Outgoing packets. Do aliasing.
 */
		PacketAliasOut (packetBuf, IP_MAXPACKET);
	}
	else {
/*
 * Do aliasing.
 */	
		PacketAliasIn (packetBuf, IP_MAXPACKET);
	}
/*
 * Length might have changed during aliasing.
 */
	bytes = ntohs (ip->ip_len);
/*
 * Update alias overhead size for outgoing packets.
 */
	if (packetAddr.sin_addr.s_addr == INADDR_ANY &&
	    bytes - origBytes > aliasOverhead)
		aliasOverhead = bytes - origBytes;

	if (verbose) {
		
/*
 * Print addresses after aliasing.
 */
		printf (" aliased to\n");
		printf ("           ");
		PrintPacket (ip);
		printf ("\n");
	}

	packetLen  = bytes;
	packetSock = fd;
	FlushPacketBuffer (fd);
}

static void FlushPacketBuffer (int fd)
{
	int			wrote;
	char			msgBuf[80];
/*
 * Put packet back for processing.
 */
	wrote = sendto (fd, 
		        packetBuf,
	    		packetLen,
	    		0,
	    		(struct sockaddr*) &packetAddr,
	    		sizeof packetAddr);
	
	if (wrote != packetLen) {
/*
 * If buffer space is not available,
 * just return. Main loop will take care of 
 * retrying send when space becomes available.
 */
		if (errno == ENOBUFS)
			return;

		if (errno == EMSGSIZE) {

			if (packetAddr.sin_addr.s_addr == INADDR_ANY &&
			    ifMTU != -1)
				SendNeedFragIcmp (icmpSock,
						  (struct ip*) packetBuf,
						  ifMTU - aliasOverhead);
		}
		else {

			sprintf (msgBuf, "Failed to write packet back.");
			Warn (msgBuf);
		}
	}

	packetSock = -1;
}

static void HandleRoutingInfo (int fd)
{
	int			bytes;
	struct if_msghdr	ifMsg;
/*
 * Get packet from socket.
 */
	bytes = read (fd, &ifMsg, sizeof ifMsg);
	if (bytes == -1) {

		Warn ("Read from routing socket failed.");
		return;
	}

	if (ifMsg.ifm_version != RTM_VERSION) {

		Warn ("Unexpected packet read from routing socket.");
		return;
	}

	if (verbose)
		printf ("Routing message %X received.\n", ifMsg.ifm_type);

	if (ifMsg.ifm_type != RTM_NEWADDR)
		return;

	if (verbose && ifMsg.ifm_index == ifIndex)
		printf ("Interface address has changed.\n");

	if (ifMsg.ifm_index == ifIndex)
		assignAliasAddr = 1;
}

static void PrintPacket (struct ip* ip)
{
	struct tcphdr*	tcphdr;

	if (ip->ip_p == IPPROTO_TCP)
		tcphdr = (struct tcphdr*) ((char*) ip + (ip->ip_hl << 2));
	else
		tcphdr = NULL;

	printf ("%s", inet_ntoa (ip->ip_src));
	if (tcphdr)
		printf (":%d", ntohs (tcphdr->th_sport));

	printf (" -> ");
	printf ("%s", inet_ntoa (ip->ip_dst));
	if (tcphdr)
		printf (":%d", ntohs (tcphdr->th_dport));
}

static void SetAliasAddressFromIfName (char* ifName)
{
	struct ifconf		cf;
	struct ifreq		buf[32];
	char			msg[80];
	struct ifreq*		ifPtr;
	int			extra;
	int			helperSock;
	int			bytes;
	struct sockaddr_in*	addr;
	int			found;
	struct ifreq		req;
	char			last[10];
/*
 * Create a dummy socket to access interface information.
 */
	helperSock = socket (AF_INET, SOCK_DGRAM, 0);
	if (helperSock == -1) {

		Quit ("Failed to create helper socket.");
		exit (1);
	}

	cf.ifc_len = sizeof (buf);
	cf.ifc_req = buf;
/*
 * Get interface data.
 */
	if (ioctl (helperSock, SIOCGIFCONF, &cf) == -1) {

		Quit ("Ioctl SIOCGIFCONF failed.");
		exit (1);
	}

	ifIndex	= 0;
	ifPtr	= buf;
	bytes	= cf.ifc_len;
	found   = 0;
	last[0] = '\0';
/*
 * Loop through interfaces until one with
 * given name is found. This is done to
 * find correct interface index for routing
 * message processing.
 */
	while (bytes) {

		if (ifPtr->ifr_addr.sa_family == AF_INET &&
                    !strcmp (ifPtr->ifr_name, ifName)) {

			found = 1;
			break;
		}

		if (strcmp (last, ifPtr->ifr_name)) {

			strcpy (last, ifPtr->ifr_name);
			++ifIndex;
		}

		extra = ifPtr->ifr_addr.sa_len - sizeof (struct sockaddr);

		ifPtr++;
		ifPtr = (struct ifreq*) ((char*) ifPtr + extra);
		bytes -= sizeof (struct ifreq) + extra;
	}

	if (!found) {

		close (helperSock);
		sprintf (msg, "Unknown interface name %s.\n", ifName);
		Quit (msg);
	}
/*
 * Get MTU size.
 */
	strcpy (req.ifr_name, ifName);

	if (ioctl (helperSock, SIOCGIFMTU, &req) == -1)
		Quit ("Cannot get interface mtu size.");

	ifMTU = req.ifr_mtu;
/*
 * Get interface address.
 */
	if (ioctl (helperSock, SIOCGIFADDR, &req) == -1)
		Quit ("Cannot get interface address.");

	addr = (struct sockaddr_in*) &req.ifr_addr;
	SetPacketAliasAddress (addr->sin_addr);
	syslog (LOG_INFO, "Aliasing to %s, mtu %d bytes",
			  inet_ntoa (addr->sin_addr),
			  ifMTU);

	close (helperSock);
}

void Quit (char* msg)
{
	Warn (msg);
	exit (1);
}

void Warn (char* msg)
{
	if (background)
		syslog (LOG_ALERT, "%s (%m)", msg);
	else
		perror (msg);
}

static void RefreshAddr ()
{
	signal (SIGHUP, RefreshAddr);
	if (ifName)
		assignAliasAddr = 1;
}

static void InitiateShutdown ()
{
/*
 * Start timer to allow kernel gracefully
 * shutdown existing connections when system
 * is shut down.
 */
	signal (SIGALRM, Shutdown);
	alarm (10);
}

static void Shutdown ()
{
	running = 0;
}

/* 
 * Different options recognized by this program.
 */

enum Option {

	PacketAliasOption,
	Verbose,
	InPort,
	OutPort,
	Port,
	AliasAddress,
	InterfaceName,
	PermanentLink,
	RedirectPort,
	RedirectAddress,
	ConfigFile,
	DynamicMode
};

enum Param {
	
	YesNo,
	Numeric,
	String,
	None,
	Address,
	Service
};

/*
 * Option information structure (used by ParseOption).
 */

struct OptionInfo {
	
	enum Option		type;
	int			packetAliasOpt;
	enum Param		parm;
	char*			parmDescription;
	char*			description;
	char*			name; 
	char*			shortName;
};

/*
 * Table of known options.
 */

static struct OptionInfo optionTable[] = {

	{ PacketAliasOption,
		PKT_ALIAS_UNREGISTERED_ONLY,
		YesNo,
		"[yes|no]",
		"alias only unregistered addresses",
		"unregistered_only",
		"u" },

	{ PacketAliasOption,
		PKT_ALIAS_LOG,
		YesNo,
		"[yes|no]",
		"enable logging",
		"log",
		"l" },

	{ PacketAliasOption,
		PKT_ALIAS_DENY_INCOMING,
		YesNo,
		"[yes|no]",
		"allow incoming connections",
		"deny_incoming",
		"d" },

	{ PacketAliasOption,
		PKT_ALIAS_USE_SOCKETS,
		YesNo,
		"[yes|no]",
		"use sockets to inhibit port conflict",
		"use_sockets",
		"s" },

	{ PacketAliasOption,
		PKT_ALIAS_SAME_PORTS,
		YesNo,
		"[yes|no]",
		"try to keep original port numbers for connections",
		"same_ports",
		"m" },

	{ Verbose,
		0,
		YesNo,
		"[yes|no]",
		"verbose mode, dump packet information",
		"verbose",
		"v" },
	
	{ DynamicMode,
		0,
		YesNo,
		"[yes|no]",
		"dynamic mode, automatically detect interface address changes",
		"dynamic",
		NULL },
	
	{ InPort,
		0,
		Service,
		"number|service_name",
		"set port for incoming packets",
		"in_port",
		"i" },
	
	{ OutPort,
		0,
		Service,
		"number|service_name",
		"set port for outgoing packets",
		"out_port",
		"o" },
	
	{ Port,
		0,
		Service,
		"number|service_name",
		"set port (defaults to natd/divert)",
		"port",
		"p" },
	
	{ AliasAddress,
		0,
		Address,
		"x.x.x.x",
		"address to use for aliasing",
		"alias_address",
		"a" },
	
	{ InterfaceName,
		0,
		String,
	        "network_if_name",
		"take aliasing address from interface",
		"interface",
		"n" },

	{ PermanentLink,
		0,
		String,
	        "tcp|udp src:port dst:port alias",
		"define permanent link for incoming connection",
		"permanent_link",
		NULL },

	{ RedirectPort,
		0,
		String,
	        "tcp|udp local_addr:local_port [public_addr:]public_port"
	 	" [remote_addr[:remote_port]]",
		"redirect a port for incoming traffic",
		"redirect_port",
		NULL },

	{ RedirectAddress,
		0,
		String,
	        "local_addr public_addr",
		"define mapping between local and public addresses",
		"redirect_address",
		NULL },

	{ ConfigFile,
		0,
		String,
		"file_name",
		"read options from configuration file",
		"config",
		"f" }
};
	
static void ParseOption (char* option, char* parms, int cmdLine)
{
	int			i;
	struct OptionInfo*	info;
	int			yesNoValue;
	int			aliasValue;
	int			numValue;
	char*			strValue;
	struct in_addr		addrValue;
	int			max;
	char*			end;
/*
 * Find option from table.
 */
	max = sizeof (optionTable) / sizeof (struct OptionInfo);
	for (i = 0, info = optionTable; i < max; i++, info++) {

		if (!strcmp (info->name, option))
			break;

		if (info->shortName)
			if (!strcmp (info->shortName, option))
				break;
	}

	if (i >= max) {

		fprintf (stderr, "Unknown option %s.\n", option);
		Usage ();
	}

	yesNoValue	= 0;
	numValue	= 0;
	strValue	= NULL;
/*
 * Check parameters.
 */
	switch (info->parm) {
	case YesNo:
		if (!parms)
			parms = "yes";

		if (!strcmp (parms, "yes"))
			yesNoValue = 1;
		else
			if (!strcmp (parms, "no"))
				yesNoValue = 0;
			else {

				fprintf (stderr, "%s needs yes/no parameter.\n",
						 option);
				exit (1);
			}
		break;

	case Service:
		if (!parms) {

			fprintf (stderr, "%s needs service name or "
					 "port number  parameter.\n",
					 option);
			exit (1);
		}

		numValue = StrToPort (parms, "divert");
		break;

	case Numeric:
		if (parms)
			numValue = strtol (parms, &end, 10);
		else
			end = parms;

		if (end == parms) {

			fprintf (stderr, "%s needs numeric parameter.\n",
					 option);
			exit (1);
		}
		break;

	case String:
		strValue = parms;
		if (!strValue) {

			fprintf (stderr, "%s needs parameter.\n",
					 option);
			exit (1);
		}
		break;

	case None:
		if (parms) {

			fprintf (stderr, "%s does not take parameters.\n",
					 option);
			exit (1);
		}
		break;

	case Address:
		if (!parms) {

			fprintf (stderr, "%s needs address/host parameter.\n",
					 option);
			exit (1);
		}

		StrToAddr (parms, &addrValue);
		break;
	}

	switch (info->type) {
	case PacketAliasOption:
	
		aliasValue = yesNoValue ? info->packetAliasOpt : 0;
		PacketAliasSetMode (aliasValue, info->packetAliasOpt);
		break;

	case Verbose:
		verbose = yesNoValue;
		break;

	case DynamicMode:
		dynamicMode = yesNoValue;
		break;

	case InPort:
		inPort = numValue;
		break;

	case OutPort:
		outPort = numValue;
		break;

	case Port:
		inOutPort = numValue;
		break;

	case AliasAddress:
		memcpy (&aliasAddr, &addrValue, sizeof (struct in_addr));
		break;

	case PermanentLink:
		SetupPermanentLink (strValue);
		break;

	case RedirectPort:
		SetupPortRedirect (strValue);
		break;

	case RedirectAddress:
		SetupAddressRedirect (strValue);
		break;

	case InterfaceName:
		if (ifName)
			free (ifName);

		ifName = strdup (strValue);
		assignAliasAddr = 1;
		break;

	case ConfigFile:
		ReadConfigFile (strValue);
		break;
	}
}

void ReadConfigFile (char* fileName)
{
	FILE*	file;
	char	buf[128];
	char*	ptr;
	char*	option;

	file = fopen (fileName, "r");
	if (!file) {

		sprintf (buf, "Cannot open config file %s.\n", fileName);
		Quit (buf);
	}

	while (fgets (buf, sizeof (buf), file)) {

		ptr = strchr (buf, '\n');
		if (!ptr) {

			fprintf (stderr, "config line too link: %s\n", buf);
			exit (1);
		}

		*ptr = '\0';
		if (buf[0] == '#')
			continue;

		ptr = buf;
/*
 * Skip white space at beginning of line.
 */
		while (*ptr && isspace (*ptr))
			++ptr;

		if (*ptr == '\0')
			continue;
/*
 * Extract option name.
 */
		option = ptr;
		while (*ptr && !isspace (*ptr))
			++ptr;

		if (*ptr != '\0') {

			*ptr = '\0';
			++ptr;
		}
/*
 * Skip white space between name and parms.
 */
		while (*ptr && isspace (*ptr))
			++ptr;

		ParseOption (option, *ptr ? ptr : NULL, 0);
	}

	fclose (file);
}

static void Usage ()
{
	int			i;
	int			max;
	struct OptionInfo*	info;

	fprintf (stderr, "Recognized options:\n\n");

	max = sizeof (optionTable) / sizeof (struct OptionInfo);
	for (i = 0, info = optionTable; i < max; i++, info++) {

		fprintf (stderr, "-%-20s %s\n", info->name,
						info->parmDescription);

		if (info->shortName)
			fprintf (stderr, "-%-20s %s\n", info->shortName,
							info->parmDescription);

		fprintf (stderr, "      %s\n\n", info->description);
	}

	exit (1);
}

void SetupPermanentLink (char* parms)
{
	char		buf[128];
	char*		ptr;
	struct in_addr	srcAddr;
	struct in_addr	dstAddr;
	int		srcPort;
	int		dstPort;
	int		aliasPort;
	int		proto;
	char*		protoName;

	strcpy (buf, parms);
/*
 * Extract protocol.
 */
	protoName = strtok (buf, " \t");
	if (!protoName) {

		fprintf (stderr, "permanent_link: missing protocol.\n");
		exit (1);
	}

	proto = StrToProto (protoName);
/*
 * Extract source address.
 */
	ptr = strtok (NULL, " \t");
	if (!ptr) {

		fprintf (stderr, "permanent_link: missing src address.\n");
		exit (1);
	}

	srcPort = StrToAddrAndPort (ptr, &srcAddr, protoName);
/*
 * Extract destination address.
 */
	ptr = strtok (NULL, " \t");
	if (!ptr) {

		fprintf (stderr, "permanent_link: missing dst address.\n");
		exit (1);
	}

	dstPort = StrToAddrAndPort (ptr, &dstAddr, protoName);
/*
 * Export alias port.
 */
	ptr = strtok (NULL, " \t");
	if (!ptr) {

		fprintf (stderr, "permanent_link: missing alias port.\n");
		exit (1);
	}

	aliasPort = StrToPort (ptr, protoName);

	PacketAliasPermanentLink (srcAddr,
				  srcPort,
				  dstAddr,
				  dstPort,
				  aliasPort,
				  proto);
}

void SetupPortRedirect (char* parms)
{
	char		buf[128];
	char*		ptr;
	struct in_addr	localAddr;
	struct in_addr	publicAddr;
	struct in_addr	remoteAddr;
	int		localPort;
	int		publicPort;
	int		remotePort;
	int		proto;
	char*		protoName;
	char*		separator;

	strcpy (buf, parms);
/*
 * Extract protocol.
 */
	protoName = strtok (buf, " \t");
	if (!protoName) {

		fprintf (stderr, "redirect_port: missing protocol.\n");
		exit (1);
	}

	proto = StrToProto (protoName);
/*
 * Extract local address.
 */
	ptr = strtok (NULL, " \t");
	if (!ptr) {

		fprintf (stderr, "redirect_port: missing local address.\n");
		exit (1);
	}

	localPort = StrToAddrAndPort (ptr, &localAddr, protoName);
/*
 * Extract public port and optinally address.
 */
	ptr = strtok (NULL, " \t");
	if (!ptr) {

		fprintf (stderr, "redirect_port: missing public port.\n");
		exit (1);
	}

	separator = strchr (ptr, ':');
	if (separator)
		publicPort = StrToAddrAndPort (ptr, &publicAddr, protoName);
	else {

		publicAddr.s_addr = INADDR_ANY;
		publicPort = StrToPort (ptr, protoName);
	}

/*
 * Extract remote address and optionally port.
 */
	ptr = strtok (NULL, " \t");
	if (ptr) {


		separator = strchr (ptr, ':');
		if (separator)
			remotePort = StrToAddrAndPort (ptr,
						       &remoteAddr,
						       protoName);
		else {

			remotePort = 0;
			StrToAddr (ptr, &remoteAddr);
		}
	}
	else {

		remotePort = 0;
		remoteAddr.s_addr = INADDR_ANY;
	}

	PacketAliasRedirectPort (localAddr,
				 localPort,
				 remoteAddr,
				 remotePort,
				 publicAddr,
				 publicPort,
				 proto);
}

void SetupAddressRedirect (char* parms)
{
	char		buf[128];
	char*		ptr;
	struct in_addr	localAddr;
	struct in_addr	publicAddr;

	strcpy (buf, parms);
/*
 * Extract local address.
 */
	ptr = strtok (buf, " \t");
	if (!ptr) {

		fprintf (stderr, "redirect_address: missing local address.\n");
		exit (1);
	}

	StrToAddr (ptr, &localAddr);
/*
 * Extract public address.
 */
	ptr = strtok (NULL, " \t");
	if (!ptr) {

		fprintf (stderr, "redirect_address: missing public address.\n");
		exit (1);
	}

	StrToAddr (ptr, &publicAddr);
	PacketAliasRedirectAddr (localAddr, publicAddr);
}

void StrToAddr (char* str, struct in_addr* addr)
{
	struct hostent* hp;

	if (inet_aton (str, addr))
		return;

	hp = gethostbyname (str);
	if (!hp) {

		fprintf (stderr, "Unknown host %s.\n", str);
		exit (1);
	}

	memcpy (addr, hp->h_addr, sizeof (struct in_addr));
}

int StrToPort (char* str, char* proto)
{
	int		port;
	struct servent*	sp;
	char*		end;

	port = strtol (str, &end, 10);
	if (end != str)
		return htons (port);

	sp = getservbyname (str, proto);
	if (!sp) {

		fprintf (stderr, "Unknown service %s/%s.\n",
				 str, proto);
		exit (1);
	}

	return sp->s_port;
}

int StrToProto (char* str)
{
	if (!strcmp (str, "tcp"))
		return IPPROTO_TCP;

	if (!strcmp (str, "udp"))
		return IPPROTO_UDP;

	fprintf (stderr, "Unknown protocol %s. Expected tcp or udp.\n", str);
	exit (1);
}

int StrToAddrAndPort (char* str, struct in_addr* addr, char* proto)
{
	char*	ptr;

	ptr = strchr (str, ':');
	if (!ptr) {

		fprintf (stderr, "%s is missing port number.\n", str);
		exit (1);
	}

	*ptr = '\0';
	++ptr;

	StrToAddr (str, addr);
	return StrToPort (ptr, proto);
}

