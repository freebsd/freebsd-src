/*-
 * Copyright (c) 2013 SRI International
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
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <machine/cheri.h>
#include <machine/cheric.h>

#include <cheri/sandbox.h>

#include <tcpdump-helper.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "netdissect.h"
#include "interface.h"
#include "print.h"

#define SB_ERROR	-1
#define	SB_SHUTDOWN	0
#define	SB_RUNNING	1

int	sb_state = SB_SHUTDOWN;

u_int32_t g_localnet;
u_int32_t g_mask;
int g_type = -1;

static struct sandbox_class	*tcpdump_classp;
static struct sandbox_object	*tcpdump_objectp;
static struct cheri_object	 tcpdump_systemcap;

static int
sb_create()
{

	if (sandbox_class_new("/usr/libexec/tcpdump-helper.bin",
	    8*1024*1024, &tcpdump_classp) < 0) {
		fprintf(stderr, "failed to create sandbox class: %s",
		    strerror(errno));
		sb_state = SB_ERROR;
		return (-1);
	}
	if (sandbox_object_new(tcpdump_classp, &tcpdump_objectp) < 0) {
		fprintf(stderr, "failed to create sandbox object: %s",
		    strerror(errno));
		sb_state = SB_ERROR;
		return (-1);
	}
	(void)sandbox_class_method_declare(tcpdump_classp,
	    TCPDUMP_HELPER_OP_INIT, "init");
	(void)sandbox_class_method_declare(tcpdump_classp,
	    TCPDUMP_HELPER_OP_PRINT_PACKET, "print_packet");
	(void)sandbox_class_method_declare(tcpdump_classp,
	    TCPDUMP_HELPER_OP_HAS_PRINTER, "has_printer");
	cheri_systemcap_get(&tcpdump_systemcap);

	return (0);
}

static int
sb_init(netdissect_options *ndo)
{
	int ret;

	if (tcpdump_classp == NULL &&
	    sb_create() != 0)
		return (-1);

	ndo->ndo_dlt = g_type;

	ret = sandbox_object_cinvoke(tcpdump_objectp,
	    TCPDUMP_HELPER_OP_INIT, g_localnet, g_mask, 0, 0, 0, 0, 0,
	    tcpdump_systemcap.co_codecap, tcpdump_systemcap.co_datacap,
	    cheri_ptrperm((void *)ndo, sizeof(*ndo), CHERI_PERM_LOAD),
	    ndo->ndo_espsecret == NULL ? cheri_zerocap() :
	    cheri_ptrperm((void *)ndo->ndo_espsecret, strlen(ndo->ndo_espsecret),
		CHERI_PERM_LOAD),
	    cheri_zerocap(), cheri_zerocap(), cheri_zerocap(),
	    cheri_zerocap());
	if (ret != 0)
		fprintf(stderr, "failed to initalizse sandbox: %d \n", ret);

	sb_state = SB_RUNNING;

	return (ret);
}

void
init_print(u_int32_t localnet, u_int32_t mask)
{

	g_localnet = localnet;
	g_mask = mask;
}

int
has_printer(int type)
{

	if (tcpdump_classp == NULL &&
	    sb_create() != 0)
		return (0);	/* XXX: should error? */

	return (sandbox_object_cinvoke(tcpdump_objectp,
	    TCPDUMP_HELPER_OP_HAS_PRINTER, type, 0, 0, 0, 0, 0, 0,
	    tcpdump_systemcap.co_codecap, tcpdump_systemcap.co_datacap,
	    cheri_zerocap(), cheri_zerocap(), cheri_zerocap(),
	    cheri_zerocap(), cheri_zerocap(), cheri_zerocap()));
}

struct print_info
get_print_info(int type)
{
        struct print_info printinfo;

	/* XXX: do or retrigger sandbox init if running */
	g_type = type;

	/* Not used, contents don't matter */
	memset(&printinfo, 0, sizeof(printinfo));

	return printinfo;
}

int
pretty_print_packet(struct print_info *print_info, const struct pcap_pkthdr *h,
    const u_char *sp)
{

	if (sb_state != SB_RUNNING)
		if (sb_init(print_info->ndo) < 1)
			exit(1);

	return (sandbox_object_cinvoke(tcpdump_objectp,
	    TCPDUMP_HELPER_OP_PRINT_PACKET, 0, 0, 0, 0, 0, 0, 0,
	    tcpdump_systemcap.co_codecap, tcpdump_systemcap.co_datacap,
	    cheri_zerocap(), cheri_zerocap(),
	    cheri_ptrperm((void *)h, sizeof(*h), CHERI_PERM_LOAD),
	    cheri_ptrperm((void *)sp, h->caplen, CHERI_PERM_LOAD),
	    cheri_zerocap(), cheri_zerocap()));

	return (0);
}

/*
 * The following functions are stubs and should never be called outside
 * the print code.  They exist to limit modifictions to the core code.
 */
int
tcpdump_printf(netdissect_options *ndo _U_, const char *fmt, ...)
{
	
	abort();
}

void
ndo_default_print(netdissect_options *ndo _U_, const u_char *bp, u_int length)
{
	
	abort();
}

void
ndo_error(netdissect_options *ndo _U_, const char *fmt, ...)
{

	abort();
}

void
ndo_warning(netdissect_options *ndo _U_, const char *fmt, ...)
{

	abort();
}
