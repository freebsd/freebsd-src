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
 * Server Cache Synchronization Protocol (SCSP) Support
 * ----------------------------------------------------
 *
 * Configuration file processing
 *
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netatm/port.h> 
#include <netatm/queue.h> 
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
#include <syslog.h>

#include "scsp_msg.h"
#include "scsp_if.h"
#include "scsp_var.h"

#ifndef lint
__RCSID("@(#) $FreeBSD$");
#endif


extern int	yyparse __P((void));

/*
 * Global variables
 */
FILE		*cfg_file;
Scsp_server	*current_server;
Scsp_dcs	*current_dcs;


/*
 * Process the configuration file
 *
 * This routine is called when the daemon starts, and it can also be
 * called while it is running, as the result of a SIGHUP signal.  It
 * therefore has to be capable of both configuring the daemon from
 * scratch and modifying the configuration of a running daemon.
 *
 * Arguments:
 *	cfn	configuration file name
 *
 * Returns:
 *	0	configuration read with no errors
 *	else	error found in configuration file
 *
 */
int
scsp_config(cfn)
	char	*cfn;
{
	int		rc;
	Scsp_server	*ssp, *snext;

	/*
	 * Open the configuration file
	 */
	cfg_file = fopen(cfn, "r");
	if (!cfg_file) {
		scsp_log(LOG_ERR, "can't open config file %s",
				(void *)cfn);
		exit(1);
	}

	/*
	 * Initialize current interface pointer
	 */
	current_server = (Scsp_server *)0;

	/*
	 * Clear marks on any existing servers
	 */
	for (ssp = scsp_server_head; ssp; ssp = ssp->ss_next) {
		ssp->ss_mark = 0;
	}

	/*
	 * Scan the configuration file, processing each line as
	 * it is read
	 */
	rc = yyparse();

	/*
	 * Close the configuration file
	 */
	fclose(cfg_file);

	/*
	 * Delete any server entries that weren't updated
	 */
	for (ssp = scsp_server_head; ssp; ssp = snext) {
		snext = ssp->ss_next;
		if (!ssp->ss_mark)
			scsp_server_delete(ssp);
	}

	return(rc);
}


/*
 * Prepare for SCSP DCS setup
 *
 * This routine is called from yyparse() when a DCS command is found.
 *
 * Arguments:
 *	none
 *
 * Returns:
 *	0	success
 *	1	error encountered
 *
 */
int
start_dcs()
{
	Scsp_dcs		*dcsp;

	/*
	 * Make sure we have a current server block
	 */
	if (!current_server) {
		parse_error("server not found");
		return(1);
	}

	/*
	 * Allocate a DCS block
	 */
	dcsp = (Scsp_dcs *)UM_ALLOC(sizeof(Scsp_dcs));
	if (!dcsp) {
		scsp_mem_err("start_dcs: sizeof(Scsp_dcs)");
	}
	UM_ZERO(dcsp, sizeof(Scsp_dcs));

	/*
	 * Fill out DCS links and default values
	 */
	dcsp->sd_server = current_server;
	dcsp->sd_addr.address_format = T_ATM_ABSENT;
	dcsp->sd_subaddr.address_format = T_ATM_ABSENT;
	dcsp->sd_sock = -1;
	dcsp->sd_ca_rexmt_int = SCSP_CAReXmitInterval;
	dcsp->sd_csus_rexmt_int = SCSP_CSUSReXmitInterval;
	dcsp->sd_hops = SCSP_CSA_HOP_CNT;
	dcsp->sd_csu_rexmt_int = SCSP_CSUReXmitInterval;
	dcsp->sd_csu_rexmt_max = SCSP_CSUReXmitMax;
	LINK2TAIL(dcsp, Scsp_dcs, current_server->ss_dcs, sd_next);

	current_dcs = dcsp;
	return(0);
}


/*
 * Finish up server configuration
 *
 * This routine is called from yyparse() to at the end of a DCS
 * command.  It checks that required fields are set and finishes
 * up the DCS block.
 *
 * Arguments:
 *	none
 *
 * Returns:
 *	0	success
 *	1	error encountered
 *
 */
int
finish_dcs()
{
	int		rc = 0;
	Scsp_dcs	*dcsp;
	Scsp_server	*ssp;

	/*
	 * Make sure we have a current server block and DCS block
	 */
	if (!current_server) {
		parse_error("server not found");
		return(1);
	}

	if (!current_dcs) {
		parse_error("server not found");
		return(1);
	}
	ssp = current_server;
	dcsp = current_dcs;

	/*
	 * Make sure the DCS ID is set
	 */
	if (dcsp->sd_dcsid.id_len == 0) {
		parse_error("DCS ID not set");
		rc++;
	}

	/*
	 * Make sure the ATM address is set
	 */
	if (dcsp->sd_addr.address_format == T_ATM_ABSENT) {
		parse_error("DCS ATM address not set");
		rc++;
	}

	current_dcs = (Scsp_dcs *)0;
	return(rc);
}


/*
 * Configure DCS ATM address
 *
 * This routine is called from yyparse() to process an ATMaddr command.
 *
 * Arguments:
 *	ap	pointer to DCS's ATM address (in ASCII)
 *	sap	pointer to DCS's ATM subaddress (in ASCII)
 *
 * Returns:
 *	0	success
 *	1	error encountered
 *
 */
int
set_dcs_addr(ap, sap)
	char	*ap, *sap;
{
	Scsp_dcs		*dcsp;
	Atm_addr		addr, subaddr;

	/*
	 * Make sure we have a current server block and DCS block
	 */
	if (!current_server) {
		parse_error("server not found");
		return(1);
	}

	if (!current_dcs) {
		parse_error("server not found");
		return(1);
	}
	dcsp = current_dcs;

	/*
	 * Initialize
	 */
	UM_ZERO(&addr, sizeof(addr));
	addr.address_format = T_ATM_ABSENT;
	UM_ZERO(&subaddr, sizeof(subaddr));
	subaddr.address_format = T_ATM_ABSENT;

	/*
	 * Convert the ATM address from character to internal format
	 */
	if (ap) {
		addr.address_length = get_hex_atm_addr(ap,
				(u_char *)addr.address, strlen(ap));
		if (addr.address_length == 0) {
			parse_error("invalid ATM address");
			return(1);
		}
		if (addr.address_length == sizeof(Atm_addr_nsap)) {
			addr.address_format = T_ATM_ENDSYS_ADDR;
		} else if (addr.address_length <=
				sizeof(Atm_addr_e164)) {
			addr.address_format = T_ATM_E164_ADDR;
		} else {
			parse_error("invalid ATM address");
			return(1);
		}
	}

	/*
	 * Convert the ATM subaddress from character to internal format
	 */
	if (sap) {
		subaddr.address_length = get_hex_atm_addr(sap,
				(u_char *)subaddr.address, strlen(sap));
		if (subaddr.address_length == 0) {
			parse_error("invalid ATM address");
			return(1);
		}
		if (subaddr.address_length == sizeof(Atm_addr_nsap)) {
			subaddr.address_format = T_ATM_ENDSYS_ADDR;
		} else if (subaddr.address_length <=
				sizeof(Atm_addr_e164)) {
			subaddr.address_format = T_ATM_E164_ADDR;
		} else {
			parse_error("invalid ATM subaddress");
			return(1);
		}
	}

	/*
	 * Make sure we have a legal ATM address type combination
	 */
	if (((addr.address_format != T_ATM_ENDSYS_ADDR) ||
			(subaddr.address_format != T_ATM_ABSENT)) &&
			((addr.address_format != T_ATM_E164_ADDR) ||
			(subaddr.address_format != T_ATM_ENDSYS_ADDR))) {
		parse_error("invalid address/subaddress combination");
		return(1);
	}

	/*
	 * Save the address and subaddress
	 */
	ATM_ADDR_COPY(&addr, &dcsp->sd_addr);
	ATM_ADDR_COPY(&subaddr, &dcsp->sd_subaddr);

	return(0);
}


/*
 * Configure CA retransmit interval for DCS
 *
 * This routine is called from yyparse() to process a CAReXmitInt
 * command.
 *
 * Arguments:
 *	val	time interval
 *
 * Returns:
 *	0	success
 *	1	error encountered
 *
 */
int
set_dcs_ca_rexmit(val)
	int	val;
{
	Scsp_dcs	*dcsp;

	/*
	 * Make sure we have a current server block and DCS block
	 */
	if (!current_server) {
		parse_error("server not found");
		return(1);
	}

	if (!current_dcs) {
		parse_error("server not found");
		return(1);
	}
	dcsp = current_dcs;


	/*
	 * Validate the interval
	 */
	if (val <= 0 || val > 1024) {
		parse_error("invalid CA retransmit interval");
		return(1);
	}

	/*
	 * Set CA retransmit interval
	 */
	dcsp->sd_ca_rexmt_int = val;

	return(0);
}


/*
 * Configure CSUS retransmit interval for DCS
 *
 * This routine is called from yyparse() to process a CSUSReXmitInt
 * command.
 *
 * Arguments:
 *	val	time interval
 *
 * Returns:
 *	0	success
 *	1	error encountered
 *
 */
int
set_dcs_csus_rexmit(val)
	int	val;
{
	Scsp_dcs	*dcsp;

	/*
	 * Make sure we have a current server block and DCS block
	 */
	if (!current_server) {
		parse_error("server not found");
		return(1);
	}

	if (!current_dcs) {
		parse_error("server not found");
		return(1);
	}
	dcsp = current_dcs;


	/*
	 * Validate the interval
	 */
	if (val <= 0 || val > 1024) {
		parse_error("invalid CSUS retransmit interval");
		return(1);
	}

	/*
	 * Set CSUS retransmit interval
	 */
	dcsp->sd_csus_rexmt_int = val;

	return(0);
}


/*
 * Configure CSU retransmit interval for DCS
 *
 * This routine is called from yyparse() to process a CSUReXmitInt
 * command.
 *
 * Arguments:
 *	val	time interval
 *
 * Returns:
 *	0	success
 *	1	error encountered
 *
 */
int
set_dcs_csu_rexmit(val)
	int	val;
{
	Scsp_dcs	*dcsp;

	/*
	 * Make sure we have a current server block and DCS block
	 */
	if (!current_server) {
		parse_error("server not found");
		return(1);
	}

	if (!current_dcs) {
		parse_error("server not found");
		return(1);
	}
	dcsp = current_dcs;


	/*
	 * Validate the interval
	 */
	if (val <= 0 || val > 1024) {
		parse_error("invalid CSU retransmit interval");
		return(1);
	}

	/*
	 * Set CSU retransmit interval
	 */
	dcsp->sd_csu_rexmt_int = val;

	return(0);
}


/*
 * Configure CSU retransmit limit for DCS
 *
 * This routine is called from yyparse() to process a CSUReXmitMax
 * command.
 *
 * Arguments:
 *	val	time interval
 *
 * Returns:
 *	0	success
 *	1	error encountered
 *
 */
int
set_dcs_csu_rexmit_max(val)
	int	val;
{
	Scsp_dcs	*dcsp;

	/*
	 * Make sure we have a current server block and DCS block
	 */
	if (!current_server) {
		parse_error("server not found");
		return(1);
	}

	if (!current_dcs) {
		parse_error("server not found");
		return(1);
	}
	dcsp = current_dcs;


	/*
	 * Validate the interval
	 */
	if (val <= 0 || val > 1024) {
		parse_error("invalid CSU retransmit maximum");
		return(1);
	}

	/*
	 * Set CSU retransmit limit
	 */
	dcsp->sd_csu_rexmt_max = val;

	return(0);
}


/*
 * Configure Hello dead factor for DCS
 *
 * This routine is called from yyparse() to process a HelloDead
 * command.
 *
 * Arguments:
 *	val	number of times Hello interval has to expire before
 *		a DCS is considered dead
 *
 * Returns:
 *	0	success
 *	1	error encountered
 *
 */
int
set_dcs_hello_df(val)
	int	val;
{
	Scsp_dcs	*dcsp;

	/*
	 * Make sure we have a current server block and DCS block
	 */
	if (!current_server) {
		parse_error("server not found");
		return(1);
	}

	if (!current_dcs) {
		parse_error("server not found");
		return(1);
	}
	dcsp = current_dcs;


	/*
	 * Validate the limit
	 */
	if (val <= 0 || val > 1024) {
		parse_error("invalid Hello dead factor");
		return(1);
	}

	/*
	 * Set Hello dead factor
	 */
	dcsp->sd_hello_df = val;

	return(0);
}


/*
 * Configure Hello interval for DCS
 *
 * This routine is called from yyparse() to process a HelloInt
 * command.
 *
 * Arguments:
 *	val	time interval
 *
 * Returns:
 *	0	success
 *	1	error encountered
 *
 */
int
set_dcs_hello_int(val)
	int	val;
{
	Scsp_dcs	*dcsp;

	/*
	 * Make sure we have a current server block and DCS block
	 */
	if (!current_server) {
		parse_error("server not found");
		return(1);
	}

	if (!current_dcs) {
		parse_error("server not found");
		return(1);
	}
	dcsp = current_dcs;


	/*
	 * Validate the interval
	 */
	if (val <= 0 || val > 1024) {
		parse_error("invalid Hello interval");
		return(1);
	}

	/*
	 * Set Hello interval
	 */
	dcsp->sd_hello_int = val;

	return(0);
}


/*
 * Configure hop count for SCSP server
 *
 * This routine is called from yyparse() to process a Hops command.
 *
 * Arguments:
 *	hops	number of hops
 *
 * Returns:
 *	0	success
 *	1	error encountered
 *
 */
int
set_dcs_hops(hops)
	int	hops;
{
	Scsp_dcs	*dcsp;

	/*
	 * Make sure we have a current server block and DCS block
	 */
	if (!current_server) {
		parse_error("server not found");
		return(1);
	}

	if (!current_dcs) {
		parse_error("server not found");
		return(1);
	}
	dcsp = current_dcs;


	/*
	 * Validate the count
	 */
	if (hops <= 0 || hops > 1024) {
		parse_error("invalid hop count");
		return(1);
	}

	/*
	 * Set hop count
	 */
	dcsp->sd_hops = hops;

	return(0);
}


/*
 * Configure DCS ID
 *
 * This routine is called from yyparse() to process an ID command.
 *
 * Arguments:
 *	name	pointer to DCS's DNS name or IP address (in ASCII)
 *
 * Returns:
 *	0	success
 *	1	error encountered
 *
 */
int
set_dcs_id(name)
	char	*name;
{
	Scsp_dcs		*dcsp;
	Scsp_server		*ssp;
	struct sockaddr_in	*ip_addr;

	/*
	 * Make sure we have a current server block and DCS block
	 */
	if (!current_server) {
		parse_error("server not found");
		return(1);
	}

	if (!current_dcs) {
		parse_error("server not found");
		return(1);
	}
	ssp = current_server;
	dcsp = current_dcs;

	/*
	 * Convert the DNS name or IP address
	 */
	ip_addr = get_ip_addr(name);
	if (!ip_addr) {
		parse_error("invalid DCS IP address");
		return(1);
	}

	/*
	 * Verify the address length
	 */
	if (ssp->ss_id_len != sizeof(ip_addr->sin_addr)) {
		parse_error("invalid DCS ID length");
		return(1);
	}

	/*
	 * Set the ID in the DCS block
	 */
	dcsp->sd_dcsid.id_len = ssp->ss_id_len;
	UM_COPY(&ip_addr->sin_addr, dcsp->sd_dcsid.id, ssp->ss_id_len);

	return(0);
}


/*
 * Configure network interface for SCSP server
 *
 * This routine is called from yyparse() to process a Netif command.
 * It verifies the network interface name, gets interface information
 * from the kernel, and sets the appropriate fields in the server
 * control block.
 *
 * Arguments:
 *	netif	pointer to network interface name
 *
 * Returns:
 *	0	success
 *	1	error encountered
 *
 */
int
set_intf(netif)
	char	*netif;
{
	int			rc;
	Scsp_server		*ssp;

	/*
	 * Get the current network interface address
	 */
	ssp = current_server;
	if (!ssp) {
		parse_error("Server not found");
		rc = 1;
		goto set_intf_done;
	}

	/*
	 * Make sure we're configuring a valid
	 * network interface
	 */
	rc = verify_nif_name(netif);
	if (rc == 0) {
		parse_error("%s is not a valid network interface",
				(void *)netif);
		rc = 1;
		goto set_intf_done;
	} else if (rc < 0) {
		scsp_log(LOG_ERR, "Netif name verify error");
		exit(1);
	}

	/*
	 * Save the server's network interface name
	 */
	strcpy(ssp->ss_intf, netif);
	rc = 0;

set_intf_done:
	return(rc);
}


/*
 * Configure protocol for SCSP server
 *
 * This routine is called from yyparse() to process a Protocol command.
 *
 * Arguments:
 *	proto	SCSP protocol being configured
 *
 * Returns:
 *	0	success
 *	1	error encountered
 *
 */
int
set_protocol(proto)
	int	proto;
{
	Scsp_server	*ssp;

	/*
	 * Get address of current server block
	 */
	ssp = current_server;
	if (!ssp) {
		parse_error("server not found");
		return(1);
	}

	/*
	 * Process based on protocol ID
	 */
	switch(proto) {
	case SCSP_PROTO_ATMARP:
		ssp->ss_pid = proto;
		ssp->ss_id_len = SCSP_ATMARP_ID_LEN;
		ssp->ss_ckey_len = SCSP_ATMARP_KEY_LEN;
		break;
	case SCSP_PROTO_NHRP:
		ssp->ss_pid = proto;
		ssp->ss_id_len = SCSP_NHRP_ID_LEN;
		ssp->ss_ckey_len = SCSP_NHRP_KEY_LEN;
		break;
	case SCSP_PROTO_MARS:
	case SCSP_PROTO_DHCP:
	case SCSP_PROTO_LNNI:
	default:
		parse_error("invalid protocol");
		return(1);
	}

	return(0);
}


/*
 * Configure server group for SCSP server
 *
 * This routine is called from yyparse() to process a ServerGroupID
 * command.
 *
 * Arguments:
 *	sgid	server group id
 *
 * Returns:
 *	0	success
 *	1	error encountered
 *
 */
int
set_server_group(sgid)
	int	sgid;
{
	Scsp_server	*ssp;

	/*
	 * Get address of current server block
	 */
	ssp = current_server;
	if (!ssp) {
		parse_error("server not found");
		return(1);
	}

	/*
	 * Validate server group ID
	 */
	if (sgid <= 0) {
		parse_error("invalid server group ID");
		return(1);
	}

	/*
	 * Save the ID
	 */
	ssp->ss_sgid = sgid;

	return(0);
}


/*
 * Prepare for SCSP server setup
 *
 * This routine is called from yyparse() when a Server statment is
 * found.
 *
 * Arguments:
 *	name	pointer to LIS name
 *
 * Returns:
 *	0	success
 *	else	error encountered
 *
 */
int
start_server(name)
	char	*name;
{
	int		i;
	Scsp_server	*ssp;
	Scsp_dcs	*dcsp, *next_dcs;
	Scsp_cse	*csep, *next_cse;

	/*
	 * See if we already have an entry for this name
	 */
	for (ssp = scsp_server_head; ssp; ssp = ssp->ss_next) {
		if (strcasecmp(ssp->ss_name, name) == 0)
			break;
	}

	if (ssp) {
		/*
		 * Log the fact that we're updating the entry
		 */
		scsp_log(LOG_INFO, "updating server entry for %s",
				(void *)name);

		/*
		 * Free the existing cache
		 */
		for (i = 0; i < SCSP_HASHSZ; i++) {
			for (csep = ssp->ss_cache[i]; csep;
					csep = next_cse) {
				next_cse = csep->sc_next;
				UNLINK(csep, Scsp_cse, ssp->ss_cache[i],
						sc_next);
				UM_FREE(csep);
			}
		}

		/*
		 * Delete existing DCS blocks
		 */
		for (dcsp = ssp->ss_dcs; dcsp; dcsp = next_dcs) {
			next_dcs = dcsp->sd_next;
			scsp_dcs_delete(dcsp);
		}
	} else {
		/*
		 * Get a new server entry
		 */
		ssp = (Scsp_server *)UM_ALLOC(sizeof(Scsp_server));
		if (!ssp) {
			scsp_log(LOG_ERR, "unable to allocate server entry");
			exit(1);
		}
		UM_ZERO(ssp, sizeof(Scsp_server));
		ssp->ss_sock = -1;
		ssp->ss_dcs_lsock = -1;

		/*
		 * Set the name
		 */
		ssp->ss_name = strdup(name);

		/*
		 * Link in the new interface entry
		 */
		LINK2TAIL(ssp, Scsp_server, scsp_server_head,
				ss_next);
	}

	/*
	 * If the mark is already set, this is a duplicate command
	 */
	if (ssp->ss_mark) {
		parse_error("duplicate server \"%s\"", name);
		return(1);
	}

	/*
	 * Make this the current interface
	 */
	current_server = ssp;

	return(0);
}


/*
 * Finish up server configuration
 *
 * This routine is called from yyparse() when the end of a server
 * statement is reached.  It checks that required fields are set
 * and marks the entry as processed.
 *
 * Arguments:
 *	None
 *
 * Returns:
 *	0	OK
 *	1	Error
 *
 */
int
finish_server()
{
	int		rc = 0;
	Scsp_server	*ssp;

	/*
	 * Get the current network interface address
	 */
	ssp = current_server;
	if (!ssp) {
		parse_error("Server not found");
		rc++;
	}

	/*
	 * Mark the interface as processed
	 */
	ssp->ss_mark = 1;

	/*
	 * Make sure the interface has been configured
	 */
	if (ssp->ss_intf == (char *)0) {
		parse_error("netif missing from server specification");
		rc++;
	}

	/*
	 * Make sure the protocol is set
	 */
	if (ssp->ss_pid == 0) {
		parse_error("protocol missing from server specification");
		rc++;
	}

	/*
	 * Make sure the server group is set
	 */
	if (ssp->ss_sgid == 0) {
		parse_error("server group ID missing from server specification");
		rc++;
	}

	/*
	 * Make sure at least one DCS is configured
	 */
	if (ssp->ss_dcs == (Scsp_dcs *)0) {
		parse_error("no DCS configured for server");
		rc++;
	}

	/*
	 * Mark the end of the server
	 */
	current_server = (Scsp_server *)0;

	return(rc);
}


/*
 * Configure log file for SCSP server
 *
 * This routine is called from yyparse() to process a log File command.
 *
 * Arguments:
 *	file	name of logging file
 *
 * Returns:
 *	0	success
 *	1	error encountered
 *
 */
int
set_log_file(file)
	char	*file;
{
	/*
	 * Make sure we haven't already got a log file
	 */
	if (scsp_log_file) {
		parse_error("multiple log files specified");
		return(1);
	}

	/*
	 * Open the file
	 */
	scsp_log_file = fopen(file, "a");
	if (!scsp_log_file) {
		parse_error("can't open log file");
		return(1);
	}

	return(0);
}
