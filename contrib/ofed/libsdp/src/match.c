/*
  This software is available to you under a choice of one of two
  licenses.  You may choose to be licensed under the terms of the GNU
  General Public License (GPL) Version 2, available at
  <http://www.fsf.org/copyleft/gpl.html>, or the OpenIB.org BSD
  license, available in the LICENSE.TXT file accompanying this
  software.  These details are also available at
  <http://openib.org/license.html>.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
  BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
  ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
  CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.

  Copyright (c) 2004 Topspin Communications.  All rights reserved.
  Copyright (c) 2005-2006 Mellanox Technologies Ltd.  All rights reserved.

  $Id$
*/

/*
 * system includes
 */
#if HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fnmatch.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/*
 * SDP specific includes
 */
#include "libsdp.h"

/* --------------------------------------------------------------------- */
/* library static and global variables                                   */
/* --------------------------------------------------------------------- */
extern char *program_invocation_name, *program_invocation_short_name;

static void
get_rule_str(
	struct use_family_rule *rule,
	char *buf,
	size_t len )
{
	char addr_buf[MAX_ADDR_STR_LEN];
	char ports_buf[16];
	char *target = __sdp_get_family_str( rule->target_family );
	char *prog = rule->prog_name_expr;

	/* TODO: handle IPv6 in rule */
	if ( rule->match_by_addr ) {
		if ( rule->prefixlen != 32 )
			sprintf( addr_buf, "%s/%d", inet_ntoa( rule->ipv4 ),
						rule->prefixlen );
		else
			sprintf( addr_buf, "%s", inet_ntoa( rule->ipv4 ) );
	} else {
		strcpy( addr_buf, "*" );
	}

	if ( rule->match_by_port )
		if ( rule->eport > rule->sport )
			sprintf( ports_buf, "%d", rule->sport );
		else
			sprintf( ports_buf, "%d-%d", rule->sport, rule->eport );
	else
		sprintf( ports_buf, "*" );

	snprintf( buf, len, "use %s %s %s:%s", target, prog, addr_buf, ports_buf );
}

/* return 0 if the addresses match */
static inline int
match_ipv4_addr(
	struct use_family_rule *rule,
	const struct sockaddr_in *sin )
{
	return ( rule->ipv4.s_addr !=
				( sin->sin_addr.
				  s_addr & htonl( SDP_NETMASK( rule->prefixlen ) ) ) );
}

static int
match_ip_addr_and_port(
	struct use_family_rule *rule,
	const struct sockaddr *addr_in,
	const socklen_t addrlen )
{
	const struct sockaddr_in *sin = ( const struct sockaddr_in * )addr_in;
	const struct sockaddr_in6 *sin6 = ( const struct sockaddr_in6 * )addr_in;
	struct sockaddr_in tmp_sin;
	unsigned short port;
	int match = 1;
	char addr_buf[MAX_ADDR_STR_LEN];
	const char *addr_str;
	char rule_str[512];

	if ( __sdp_log_get_level(  ) <= 3 ) {
		if ( sin6->sin6_family == AF_INET6 ) {
			addr_str =
				inet_ntop( AF_INET6, ( void * )&( sin6->sin6_addr ), addr_buf,
							  MAX_ADDR_STR_LEN );
			port = ntohs( sin6->sin6_port );
		} else {
			addr_str =
				inet_ntop( AF_INET, ( void * )&( sin->sin_addr ), addr_buf,
							  MAX_ADDR_STR_LEN );
			port = ntohs( sin->sin_port );
		}
		if ( addr_str == NULL )
			addr_str = "INVALID_ADDR";

		get_rule_str( rule, rule_str, sizeof( rule_str ) );

		__sdp_log( 3, "MATCH: matching %s:%d to %s => \n", addr_str, port,
					  rule_str );
	}

	/* We currently only support IPv4 and IPv4 embedded in IPv6 */
	if ( rule->match_by_port ) {
		if ( sin6->sin6_family == AF_INET6 )
			port = ntohs( sin6->sin6_port );
		else
			port = ntohs( sin->sin_port );

		if ( ( port < rule->sport ) || ( port > rule->eport ) ) {
			__sdp_log( 3, "NEGATIVE by port range\n" );
			match = 0;
		}
	}

	if ( match && rule->match_by_addr ) {
		if ( __sdp_sockaddr_to_sdp( addr_in, addrlen, &tmp_sin, NULL ) ||
			  match_ipv4_addr( rule, &tmp_sin ) ) {
			__sdp_log( 3, "NEGATIVE by address\n" );
			match = 0;
		}
	}

	if ( match )
		__sdp_log( 3, "POSITIVE\n" );

	return match;
}

/* return 1 on match */
static int
match_program_name(
	struct use_family_rule *rule )
{
	return !fnmatch( rule->prog_name_expr, program_invocation_short_name, 0 );
}

static use_family_t
get_family_by_first_matching_rule(
	const struct sockaddr *sin,
	const socklen_t addrlen,
	struct use_family_rule *rules )
{
	struct use_family_rule *rule;

	for ( rule = rules; rule != NULL; rule = rule->next ) {
		/* skip if not our program */
		if ( !match_program_name( rule ) )
			continue;

		/* first rule wins */
		if ( match_ip_addr_and_port( rule, sin, addrlen ) )
			return ( rule->target_family );
	}

	return ( USE_BOTH );
}

/* return the result of the first matching rule found */
use_family_t
__sdp_match_listen(
	const struct sockaddr * sin,
	const socklen_t addrlen )
{
	use_family_t target_family;

	/* if we do not have any rules we use sdp */
	if ( __sdp_config_empty(  ) )
		target_family = USE_SDP;
	else
		target_family =
			get_family_by_first_matching_rule( sin, addrlen,
														  __sdp_servers_family_rules_head );

	__sdp_log( 4, "MATCH LISTEN: => %s\n",
				  __sdp_get_family_str( target_family ) );

	return ( target_family );
}

use_family_t
__sdp_match_connect(
	const struct sockaddr * sin,
	const socklen_t addrlen )
{
	use_family_t target_family;

	/* if we do not have any rules we use sdp */
	if ( __sdp_config_empty(  ) )
		target_family = USE_SDP;
	else
		target_family =
			get_family_by_first_matching_rule( sin, addrlen,
														  __sdp_clients_family_rules_head );

	__sdp_log( 4, "MATCH CONNECT: => %s\n",
				  __sdp_get_family_str( target_family ) );

	return ( target_family );
}

/* given a set of rules see if there is a global match for current program */
static use_family_t
match_by_all_rules_program(
	struct use_family_rule *rules )
{
	int any_sdp = 0;
	int any_tcp = 0;
	use_family_t target_family = USE_BOTH;
	struct use_family_rule *rule;

	for ( rule = rules; ( rule != NULL ) && ( target_family == USE_BOTH );
			rule = rule->next ) {
		/* skip if not our program */
		if ( !match_program_name( rule ) )
			continue;

		/*
		 * to declare a dont care we either have a dont care address and port  
		 * or the previous non global rules use the same target family as the
		 * global rule
		 */
		if ( rule->match_by_addr || rule->match_by_port ) {
			/* not a glocal match rule - just track the target family */
			if ( rule->target_family == USE_SDP )
				any_sdp++;
			else if ( rule->target_family == USE_TCP )
				any_tcp++;
		} else {
			/* a global match so we can declare a match by program */
			if ( ( rule->target_family == USE_SDP ) && ( any_tcp == 0 ) )
				target_family = USE_SDP;
			else if ( ( rule->target_family == USE_TCP ) && ( any_sdp == 0 ) )
				target_family = USE_TCP;
		}
	}
	return ( target_family );
}

/* return tcp or sdp if the port and role are dont cares */
use_family_t
__sdp_match_by_program(
	 )
{
	use_family_t server_target_family;
	use_family_t client_target_family;
	use_family_t target_family = USE_BOTH;

	if ( __sdp_config_empty(  ) ) {
		target_family = USE_SDP;
	} else {
		/* need to try both server and client rules */
		server_target_family =
			match_by_all_rules_program( __sdp_servers_family_rules_head );
		client_target_family =
			match_by_all_rules_program( __sdp_clients_family_rules_head );

		/* only if both agree */
		if ( server_target_family == client_target_family )
			target_family = server_target_family;
	}

	__sdp_log( 4, "MATCH PROGRAM: => %s\n",
				  __sdp_get_family_str( target_family ) );

	return ( target_family );
}
