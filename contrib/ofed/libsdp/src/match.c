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
#include <sys/types.h>
#ifdef __linux__
#include <linux/types.h>
#elif defined(__FreeBSD__)
#define s6_addr32 __u6_addr.__u6_addr32
#define __be32 uint32_t
#endif

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
		char tmp[INET6_ADDRSTRLEN] = "BAD ADDRESS";

		if (rule->ip.ss_family == AF_INET)
			inet_ntop(AF_INET, &((struct sockaddr_in *)&rule->ip)->sin_addr, tmp, sizeof(tmp));
		else if (rule->ip.ss_family == AF_INET6)
			inet_ntop(AF_INET6, &((struct sockaddr_in6 *)&rule->ip)->sin6_addr, tmp, sizeof(tmp));

		sprintf( addr_buf, "%s/%d", tmp, rule->prefixlen);
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

static inline int __ipv6_prefix_equal(const __be32 *a1, const __be32 *a2,
				      unsigned int prefixlen)
{
	unsigned pdw, pbi;

	/* check complete u32 in prefix */
	pdw = prefixlen >> 5;
	if (pdw && memcmp(a1, a2, pdw << 2))
		return 0;

	/* check incomplete u32 in prefix */
	pbi = prefixlen & 0x1f;
	if (pbi && ((a1[pdw] ^ a2[pdw]) & htonl((0xffffffff) << (32 - pbi))))
		return 0;

	return 1;
}

static inline int ipv6_prefix_equal(const struct in6_addr *a1,
				    const struct in6_addr *a2,
				    unsigned int prefixlen)
{
	return __ipv6_prefix_equal(a1->s6_addr32, a2->s6_addr32,
				   prefixlen);
}

/* return 0 if the addresses match */
static inline int
match_addr(
	struct use_family_rule *rule,
	const struct sockaddr *addr_in )
{
	const struct sockaddr_in *sin = ( const struct sockaddr_in * )addr_in;
	const struct sockaddr_in6 *sin6 = ( const struct sockaddr_in6 * )addr_in;
	const struct sockaddr_in *rule_sin = ( const struct sockaddr_in * )(&rule->ip);
	const struct sockaddr_in6 *rule_sin6 = ( const struct sockaddr_in6 * )(&rule->ip);

	if (rule_sin->sin_family == AF_INET && !rule_sin->sin_addr.s_addr)
		return 0;

	if (addr_in->sa_family != rule->ip.ss_family)
		return -1;

	if (addr_in->sa_family == AF_INET) {
		return ( rule_sin->sin_addr.s_addr !=
				( sin->sin_addr.s_addr &
				 htonl( SDP_NETMASK( rule->prefixlen ) ) ) );
	}

	/* IPv6 */
	return !ipv6_prefix_equal(&sin6->sin6_addr, &rule_sin6->sin6_addr, rule->prefixlen);
}

static int
match_ip_addr_and_port(
	struct use_family_rule *rule,
	const struct sockaddr *addr_in,
	const socklen_t addrlen )
{
	const struct sockaddr_in *sin = ( const struct sockaddr_in * )addr_in;
	const struct sockaddr_in6 *sin6 = ( const struct sockaddr_in6 * )addr_in;
	unsigned short port;
	int match = 1;
	char addr_buf[MAX_ADDR_STR_LEN];
	const char *addr_str;
	char rule_str[512];

	if ( __sdp_log_get_level(  ) <= 3 ) {
		if ( sin6->sin6_family == AF_INET6 ) {
			addr_str = inet_ntop( AF_INET6, ( void * )&( sin6->sin6_addr ),
					addr_buf, MAX_ADDR_STR_LEN );
			port = ntohs( sin6->sin6_port );
		} else {
			addr_str = inet_ntop( AF_INET, ( void * )&( sin->sin_addr ),
					addr_buf, MAX_ADDR_STR_LEN );
			port = ntohs( sin->sin_port );
		}
		if ( addr_str == NULL )
			addr_str = "INVALID_ADDR";

		get_rule_str( rule, rule_str, sizeof( rule_str ) );

		__sdp_log( 3, "MATCH: matching %s:%d to %s => \n", addr_str, port,
					  rule_str );
	}

	if ( rule->match_by_port ) {
		port = ntohs( sin->sin_port );

		if ( ( port < rule->sport ) || ( port > rule->eport ) ) {
			__sdp_log( 3, "NEGATIVE by port range\n" );
			match = 0;
		}
	}

	if ( match && rule->match_by_addr ) {
		if ( match_addr( rule, addr_in ) ) {
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
