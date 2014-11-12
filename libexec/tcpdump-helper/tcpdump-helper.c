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

#include <cheri/cheri_system.h>

#include <stdlib.h>
#include <string.h>
#include <md5.h>

#include "cheri_tcpdump_system.h"
#include "tcpdump-helper.h"

#include "netdissect.h"
#include "interface.h"
#include "print.h"

struct print_info printinfo;
netdissect_options Gndo;
netdissect_options *gndo = &Gndo;

const char *program_name;

void	pawned(void);

int	invoke(register_t op, register_t localnet, register_t netmask,
	    struct cheri_object system_object,
	    __capability const netdissect_options *ndo,
	    __capability const char *ndo_espsecret,
	    __capability const struct pcap_pkthdr *h,
	    __capability const u_char *sp);

static int
invoke_init(bpf_u_int32 localnet, bpf_u_int32 netmask,
    __capability const netdissect_options *ndo,
    __capability const char *ndo_espsecret)
{
	size_t espsec_len;

/* XXXBD: broken, use system default
	cheri_system_methodnum_puts = CHERI_TCPDUMP_PUTS;
	cheri_system_methodnum_putchar = CHERI_TCPDUMP_PUTCHAR;
*/

	program_name = "tcpdump-helper"; /* XXX: copy from parent? */

	/*
	 * Make a copy of the parent's netdissect_options.  Most of the
	 * items are unchanged until the next init or per-packet.  The
	 * exceptions are related to IPSec decryption and we punt on
	 * those for now and allow them to be reinitalized on a
	 * per-sandbox basis.
	 */
	memcpy_c(gndo, ndo, sizeof(netdissect_options));
	if (ndo->ndo_espsecret != NULL) { /* XXX: check the real thing */
		if (gndo->ndo_espsecret != NULL)
			free(gndo->ndo_espsecret);
		
		espsec_len = cheri_getlen((__capability void *)ndo_espsecret);
		gndo->ndo_espsecret = malloc(espsec_len);
		if (gndo->ndo_espsecret == NULL)
			abort();
		memcpy_c(cheri_ptr(gndo->ndo_espsecret, espsec_len),
		    ndo_espsecret, espsec_len);
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
    struct cheri_object system_object,
    __capability const netdissect_options *ndo,
    __capability const char *ndo_espsecret,
    __capability const struct pcap_pkthdr *h, __capability const u_char *sp)
{
	int ret;

	cheri_system_setup(system_object);

	ret = 0;

	switch (op) {
	case TCPDUMP_HELPER_OP_INIT:
#ifdef DEBUG
		printf("calling invoke_init\n");
#endif
		return (invoke_init(arg1, arg2, ndo, ndo_espsecret));

	case TCPDUMP_HELPER_OP_PRINT_PACKET:
#ifdef DEBUG
		/* XXX printf broken here */
		printf("printing a packet of length 0x%x\n", h->caplen);
		printf("sp b:%016jx l:%016zx o:%jx\n",
		    cheri_getbase((void *)sp),
		    cheri_getlen((void *)sp),
		    cheri_getoffset((void *)sp));
#endif
		assert(h->caplen == cheri_getlen((__capability void *)sp));

		/*
		 * XXXBD: Hack around the need to not store the packet except
		 * on the stack.  Should really avoid this somehow...
		 */
		gndo->ndo_packetp = malloc(h->caplen);
		if (gndo->ndo_packetp == NULL)
			error("failed to malloc packet space\n");
		gndo->ndo_snapend = gndo->ndo_packetp + h->caplen;

		if (printinfo.ndo_type)
			ret = (*printinfo.p.ndo_printer)(printinfo.ndo,
			     h, gndo->ndo_packetp);
		else
			ret = (*printinfo.p.printer)(h, gndo->ndo_packetp);

		/* XXX: what else to reset? */
		free((void*)(gndo->ndo_packetp));
		gndo->ndo_packetp = NULL;
		snapend = NULL;
		break;

	case TCPDUMP_HELPER_OP_HAS_PRINTER:
		return (has_printer(arg1));

	default:
		printf("unknown op %ld\n", op);
		abort();
	}

	return (ret);
}

void
pawned(void)
{

	cheri_system_methodnum_puts = CHERI_TCPDUMP_PUTS_PAWNED;
	cheri_system_methodnum_putchar = CHERI_TCPDUMP_PUTCHAR_PAWNED;
	printf(">>> ATTACKER OUTPUT <<<");
}
