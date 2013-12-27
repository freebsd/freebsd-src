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

#include <netinet/in.h>

/*
 * SDP specific includes
 */
#include "linux/sdp_inet.h"

/* --------------------------------------------------------------------- */
/* library static and global variables                                   */
/* --------------------------------------------------------------------- */

/* max string length to store any IPv4/IPv6 address */
#define MAX_ADDR_STR_LEN 49

typedef enum
{
	USE_TCP = 1,
	USE_SDP,
	USE_BOTH,
} use_family_t;

/* some state to string functions */
static inline char *
__sdp_get_family_str(
	use_family_t family )
{
	switch ( family ) {
	case USE_TCP:
		return "tcp";
		break;
	case USE_SDP:
		return "sdp";
		break;
	case USE_BOTH:
		return "both";
		break;
	}
	return ( "unknown-family" );
}

/* data structure for holding address family mapoping rules */
/* note we filter non relevant programs during parsing ...  */
struct use_family_rule
{
	struct use_family_rule *prev, *next;
	int match_by_addr;			  /* if 0 ignore address match        */
	struct in_addr ipv4;			  /* IPv4 address for mapping         */
	unsigned char prefixlen;	  /* length of CIDR prefix (ie /24)   */
	int match_by_port;			  /* if 0 ignore port match           */
	unsigned short sport, eport; /* start port - end port, inclusive */
	use_family_t target_family;  /* if match - use this family       */
	char *prog_name_expr;		  /* expression for program name      */
};

extern struct use_family_rule *__sdp_clients_family_rules_head;
extern struct use_family_rule *__sdp_clients_family_rules_tail;
extern struct use_family_rule *__sdp_servers_family_rules_head;
extern struct use_family_rule *__sdp_servers_family_rules_tail;

#define SDP_NETMASK(n) ((n == 0) ? 0 : ~((1UL<<(32 - n)) - 1))

/* match.c */
use_family_t __sdp_match_connect(
	const struct sockaddr *sin,
	const socklen_t  addrlen );

use_family_t __sdp_match_listen(
	const struct sockaddr *sin,
	const socklen_t  addrlen );

/* config.c */
int __sdp_config_empty(
	void );

int __sdp_parse_config(
	const char *config_file );

use_family_t __sdp_match_by_program(
	 );

/* log.c */
void __sdp_log(
	int level,
	char *format,
	... );

int __sdp_log_get_level( 
	void );

void __sdp_log_set_min_level(
	int level );

int __sdp_log_set_log_stderr(
	void );

int __sdp_log_set_log_syslog(
	void );

int __sdp_log_set_log_file(
	char *filename );

/* port.c */
int __sdp_sockaddr_to_sdp(
	const struct sockaddr *addr_in,
	socklen_t addrlen,
	struct sockaddr_in *addr_out,
	int *was_ipv6 );
