%{
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
 *	@(#) $FreeBSD: src/usr.sbin/atm/scspd/scsp_config_parse.y,v 1.3 1999/08/28 01:15:32 peter Exp $
 *
 */


/*
 * Server Cache Synchronization Protocol (SCSP) Support
 * ----------------------------------------------------
 *
 * YACC input for configuration file processing
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

#include <libatm.h>
#if __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#include "scsp_msg.h"
#include "scsp_if.h"
#include "scsp_var.h"

#ifndef lint
__RCSID("@(#) $FreeBSD: src/usr.sbin/atm/scspd/scsp_config_parse.y,v 1.3 1999/08/28 01:15:32 peter Exp $");
#endif


void	yyerror __P((char *));
%}


/*
 * Token value definition
 */
%union {
	char		*tv_alpha;
	int		tv_int;
	char		*tv_hex;
}


/*
 * Token types returned by scanner
 */
%token	<tv_alpha>	TOK_NAME
%token	<tv_int>	TOK_INTEGER
%token	<tv_hex>	TOK_HEX

/*
 * Reserved words
 */
%token			TOK_ATMARP
%token			TOK_DCS
%token			TOK_DCS_ADDR
%token			TOK_DCS_CA_REXMIT_INT
%token			TOK_DCS_CSUS_REXMIT_INT
%token			TOK_DCS_CSU_REXMIT_INT
%token			TOK_DCS_CSU_REXMIT_MAX
%token			TOK_DCS_HELLO_DF
%token			TOK_DCS_HELLO_INT
%token			TOK_DCS_HOP_CNT
%token			TOK_DCS_ID
%token			TOK_DHCP
%token			TOK_FAMILY
%token			TOK_LFN
%token			TOK_LNNI
%token			TOK_LOG
%token			TOK_MARS
%token			TOK_NETIF
%token			TOK_NHRP
%token			TOK_PROTOCOL
%token			TOK_SERVER
%token			TOK_SRVGRP
%token			TOK_SYSLOG


%%
cfg_file: /* Empty */
	| stmt_seq

stmt_seq: stmt
	| stmt_seq stmt
	;

stmt:	server_stmt ';'
	| log_stmt ';'
	;

/*
 * SCSP server definition statements
 */
server_stmt: TOK_SERVER TOK_NAME
	{
		int	rc;

		rc = start_server($2);
		UM_FREE($2);
		if (rc)
			return(rc);
	}
	'{' server_def '}'
	{
		int	rc;

		rc = finish_server();
		if (rc)
			return(rc);
	}
	;

server_def: server_spec ';'
	| server_def server_spec ';'
	;

server_spec: /* Nothing */
	| dcs_stmt
	| TOK_NETIF TOK_NAME
	{
		int	rc;

		/*
		 * Configure the network interface
		 */
		rc = set_intf($2);
		UM_FREE($2);
		if (rc)
			return(rc);
	}
	| TOK_PROTOCOL TOK_ATMARP
	{
		int	rc;

		/*
		 * Configure the protocol
		 */
		rc = set_protocol(SCSP_PROTO_ATMARP);
		if (rc)
			return(rc);
	}
	| TOK_PROTOCOL TOK_DHCP | TOK_LNNI | TOK_MARS | TOK_NHRP
	{
		yyerror("Protocol not implemented");
		return(1);
	}
	| TOK_SRVGRP TOK_INTEGER
	{
		int	rc;

		/*
		 * Configure the SCSP server group ID
		 */
		rc = set_server_group($2);
		if (rc)
			return(rc);
	}
	;

/*
 * SCSP DCS definition statements
 */
dcs_stmt: TOK_DCS 
	{
		int	rc;

		rc = start_dcs();
		if (rc)
			return(rc);
	}
	'{' dcs_def '}'
	{
		int	rc;

		rc = finish_dcs();
		if (rc)
			return(rc);
	}
	;

dcs_def: dcs_spec ';'
	| dcs_def dcs_spec ';'
	;

dcs_spec: /* Nothing */
	| TOK_DCS_ADDR TOK_HEX
	{
		int	rc;

		/*
		 * Set DCS address
		 */
		rc = set_dcs_addr($2, (char *)0);
		UM_FREE($2);
		if (rc)
			return(rc);
	}
	| TOK_DCS_ADDR TOK_HEX TOK_HEX
	{
		int	rc;

		/*
		 * Set DCS address and subaddress
		 */
		rc = set_dcs_addr($2, $3);
		UM_FREE($2);
		UM_FREE($3);
		if (rc)
			return(rc);
	}
	| TOK_DCS_CA_REXMIT_INT TOK_INTEGER
	{
		int	rc;

		/*
		 * Configure the CA retransmit interval
		 */
		rc = set_dcs_ca_rexmit($2);
		if (rc)
			return(rc);
	}
	| TOK_DCS_CSUS_REXMIT_INT TOK_INTEGER
	{
		int	rc;

		/*
		 * Configure the CSUS retransmit interval
		 */
		rc = set_dcs_csus_rexmit($2);
		if (rc)
			return(rc);
	}
	| TOK_DCS_CSU_REXMIT_INT TOK_INTEGER
	{
		int	rc;

		/*
		 * Configure the CSU retransmit interval
		 */
		rc = set_dcs_csu_rexmit($2);
		if (rc)
			return(rc);
	}
	| TOK_DCS_CSU_REXMIT_MAX TOK_INTEGER
	{
		int	rc;

		/*
		 * Configure the CSU retransmit limit
		 */
		rc = set_dcs_csu_rexmit_max($2);
		if (rc)
			return(rc);
	}
	| TOK_DCS_HELLO_DF TOK_INTEGER
	{
		int	rc;

		/*
		 * Configure the Hello dead factor
		 */
		rc = set_dcs_hello_df($2);
		if (rc)
			return(rc);
	}
	| TOK_DCS_HELLO_INT TOK_INTEGER
	{
		int	rc;

		/*
		 * Configure the Hello interval
		 */
		rc = set_dcs_hello_int($2);
		if (rc)
			return(rc);
	}
	| TOK_DCS_HOP_CNT TOK_INTEGER
	{
		int	rc;

		/*
		 * Configure the hop count
		 */
		rc = set_dcs_hops($2);
		if (rc)
			return(rc);
	}
	| TOK_DCS_ID TOK_NAME
	{
		int	rc;

		/*
		 * Configure the DCS ID
		 */
		rc = set_dcs_id($2);
		UM_FREE($2);
		if (rc)
			return(rc);
	}
	;


/*
 * Logging option statements
 */
log_stmt: TOK_LOG
	'{' log_spec '}'
	;

log_spec: /* Nothing */
	| TOK_LFN TOK_NAME ';'
	{
		/*
		 * Configure the log file name
		 */
		int	rc;

		rc = set_log_file($2);
		UM_FREE($2);
		if (rc)
			return(rc);
	}
	;
	| TOK_SYSLOG ';'
	{
		/*
		 * Configure logging to syslog
		 */
		scsp_log_syslog = 1;
	}
	;

%%

void
#if __STDC__
parse_error(const char *fmt, ...)
#else
parse_error(fmt, va_alist)
	char	*fmt;
	va_dcl
#endif
{
	va_list	ap;
	char	buff[256];

#if __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif

	vsprintf(buff, fmt, ap);
	scsp_log(LOG_ERR, "%s: Config file error at line %d: %s\n",
			prog, parse_line, buff);
#ifdef NOTDEF
	fprintf(stderr, "%s: Config file error at line %d: %s\n",
			prog, parse_line, buff);
#endif
	va_end(ap);
}


void
yyerror(s)
	char	*s;
{
	parse_error(s);
}
