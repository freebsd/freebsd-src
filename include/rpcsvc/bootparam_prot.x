/*
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user.
 * 
 * SUN RPC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 * 
 * Sun RPC is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 * 
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY SUN RPC
 * OR ANY PART THEREOF.
 * 
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 * 
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */

/*
 * RPC for bootparms service.
 * There are two procedures:
 *   WHOAMI takes a net address and returns a client name and also a
 *	likely net address for routing
 *   GETFILE takes a client name and file identifier and returns the
 *	server name, server net address and pathname for the file.
 *   file identifiers typically include root, swap, pub and dump
 */

#ifdef RPC_HDR
%#include <rpc/types.h>
%#include <sys/time.h>
%#include <sys/errno.h>
%#include <sys/param.h>
%#include <sys/syslimits.h>
#else
%#ifndef lint
%/*static char sccsid[] = "from: @(#)bootparam_prot.x 1.2 87/06/24 Copyr 1987 Sun Micro";*/
%/*static char sccsid[] = "from: @(#)bootparam_prot.x	2.1 88/08/01 4.0 RPCSRC";*/
%static const char rcsid[] =
%  "$FreeBSD$";
%#endif /* not lint */
#endif

const MAX_MACHINE_NAME  = 255;
const MAX_PATH_LEN	= 1024;
const MAX_FILEID	= 32;
const IP_ADDR_TYPE	= 1;

typedef	string	bp_machine_name_t<MAX_MACHINE_NAME>;
typedef	string	bp_path_t<MAX_PATH_LEN>;
typedef	string	bp_fileid_t<MAX_FILEID>;

struct	ip_addr_t {
	char	net;
	char	host;
	char	lh;
	char	impno;
};

union bp_address switch (int address_type) {
	case IP_ADDR_TYPE:
		ip_addr_t	ip_addr;
};

struct bp_whoami_arg {
	bp_address		client_address;
};

struct bp_whoami_res {
	bp_machine_name_t	client_name;
	bp_machine_name_t	domain_name;
	bp_address		router_address;
};

struct bp_getfile_arg {
	bp_machine_name_t	client_name;
	bp_fileid_t		file_id;
};
	
struct bp_getfile_res {
	bp_machine_name_t	server_name;
	bp_address		server_address;
	bp_path_t		server_path;
};

program BOOTPARAMPROG {
	version BOOTPARAMVERS {
		bp_whoami_res	BOOTPARAMPROC_WHOAMI(bp_whoami_arg) = 1;
		bp_getfile_res	BOOTPARAMPROC_GETFILE(bp_getfile_arg) = 2;
	} = 1;
} = 100026;
