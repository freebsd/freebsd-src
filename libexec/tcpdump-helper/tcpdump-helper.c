/*-
 * Copyright (c) 2014 SRI International
 * Copyright (c) 2012-2014 Robert N. M. Watson
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Copyright (c) 1988, 1989, 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1997, 2000
 *      The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Support for splitting captures into multiple files with a maximum
 * file size:
 *
 * Copyright (c) 2001
 *      Seth Webster <swebster@sst.ll.mit.edu>
 */

#include "config.h"

#include <sys/types.h>

#include <machine/cheri.h>
#include <machine/cheric.h>

#include <cheri/cheri_memcpy.h>

#include <md5.h>
#include <stdlib.h>

#include "tcpdump-helper.h"

#include "netdissect.h"
#include "interface.h"
#include "print.h"

struct print_info printinfo;
netdissect_options Gndo;
netdissect_options *gndo = &Gndo;

const char *program_name;

int	invoke(register_t op, register_t localnet, register_t netmask,
	    __capability void *system_codecap, __capability void *system_datacap,
	    __capability const netdissect_options *ndo,
	    __capability const char *ndo_espsecret,
	    __capability const struct pcap_pkthdr *h,
	    __capability const u_char *sp);

__capability void *cmemcpy(__capability void *dst,
                            const __capability void *src,
                            size_t len);
static int
invoke_init(bpf_u_int32 localnet, bpf_u_int32 netmask,
    __capability const netdissect_options *ndo,
    __capability const char *ndo_espsecret)
{

	program_name = "tcpdump-helper"; /* XXX: copy from parent? */

	/*
	 * Make a copy of the parent's netdissect_options.  Most of the
	 * items are unchanged until the next init or per-packet.  The
	 * exceptions are related to IPSec decryption and we punt on
	 * those for now and allow them to be reinitalized on a
	 * per-sandbox basis.
	 */
	cmemcpy(cheri_ptr(gndo, sizeof(netdissect_options)), ndo,
	    sizeof(netdissect_options));
	if (ndo_espsecret != NULL) {
		if (gndo->ndo_espsecret != NULL)
			free(gndo->ndo_espsecret);
		
		gndo->ndo_espsecret =
		    malloc(cheri_getlen((__capability void *)ndo_espsecret));
		if (gndo->ndo_espsecret == NULL)
			abort();
	}
	gndo->ndo_printf = tcpdump_printf;
	gndo->ndo_default_print = ndo_default_print;
	gndo->ndo_error = ndo_error;
	gndo->ndo_warning = ndo_warning;

	init_print(localnet, netmask);

	printinfo.ndo_type = 1;
	printinfo.ndo = gndo;
	printinfo.p.ndo_printer = lookup_ndo_printer(gndo->ndo_dlt);
	if (printinfo.p.ndo_printer == NULL) {
		printinfo.p.printer = lookup_printer(gndo->ndo_dlt);
		printinfo.ndo_type = 0;
		if (printinfo.p.printer == NULL) {
			gndo->ndo_dltname =
			    pcap_datalink_val_to_name(gndo->ndo_dlt);
			if (gndo->ndo_dltname != NULL)
				error("packet printing is not supported for link type %s: use -w",
				      gndo->ndo_dltname);
		else
			error("packet printing is not supported for link type %d: use -w", gndo->ndo_dlt);
		}
	}

	return (0);
}

/*
 * Sandbox entry point.  An init method sets up global state.  
 * The print_packet method invokes the top level packet printing method
 * selected by init.
 *
 * c1 and c2 hold the system code and data capablities.  c3 holds the
 * parent's netdissect_options structure and c4 holes IPSec decryption
 * keys.  They are only used for init.  c5 holds a struct pcap_pkthdr and
 * c6 the packet body.   They are used only by print_packet.
 */
int
invoke(register_t op, register_t arg1, register_t arg2,
    __capability void *system_codecap __unused,
    __capability void *system_datacap __unused,
    __capability const netdissect_options *ndo,
    __capability const char *ndo_espsecret,
    __capability const struct pcap_pkthdr *h, __capability const u_char *sp)
{
	int ret;
	u_char *data;
	struct pcap_pkthdr hdr;

	ret = 0;

	switch (op) {
	case TCPDUMP_HELPER_OP_INIT:
		return (invoke_init(arg1, arg2, ndo, ndo_espsecret));

	case TCPDUMP_HELPER_OP_PRINT_PACKET:
		assert(h->caplen == cheri_getlen((__capability void *)sp));

		/*
		 * XXX: copy in the data for now.  snapend is evil, but
		 * widely used in the code so we need to hunt it down
		 * and replace it with a macro that can be implemented
		 * sanely in a capability world.
		 */
		if ((data = malloc(h->caplen)) == NULL)
			abort();
		cmemcpy(cheri_ptr(data, h->caplen), sp, h->caplen);
		cmemcpy((__capability struct pcap_pkthdr *)&hdr, h,
		    sizeof(struct pcap_pkthdr));

		gndo->ndo_packetp = data;
		gndo->ndo_snapend = data + h->caplen;

		/* XXX: invoke the current printer */

		/* XXX: what else to reset? */
		free(data);
		gndo->ndo_packetp = NULL;
		snapend = NULL;
		break;

	case TCPDUMP_HELPER_OP_HAS_PRINTER:
		return (has_printer(arg1));

	default:
		/* sb_panic("unknown op %d", op); */
		abort();
	}

	return (ret);
}
