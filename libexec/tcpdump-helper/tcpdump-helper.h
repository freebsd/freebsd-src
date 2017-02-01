/*-
 * Copyright (c) 2014-2015 SRI International
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

#ifndef _LIBEXEC_TCPDUMP_HELPER_H_
#define _LIBEXEC_TCPDUMP_HELPER_H_

CHERI_TCPDUMP_CCALL
int	cheri_tcpdump_sandbox_init(__capability netdissect_options *ndo,
	    bpf_u_int32 localnet, bpf_u_int32 netmask,
	    uint32_t timezone_offset,
	    struct cheri_object next_sandbox);
CHERI_TCPDUMP_CCALL
int	cheri_sandbox_has_printer(int type);
CHERI_TCPDUMP_CCALL
int	cheri_sandbox_pretty_print_packet(
	    __capability netdissect_options *ndo,
	    __capability const struct pcap_pkthdr *h,
	    __capability const u_char *sp);

CHERI_TCPDUMP_CCALL
void	cheri_tcpdump_sandbox_set_if_printer(
	    __capability netdissect_options *ndo, int type);
CHERI_TCPDUMP_CCALL
void	cheri_tcpdump_sandbox_set_function_pointers(
	    __capability netdissect_options *ndo);

#endif /* _LIBEXEC_TCPDUMP_HELPER_H_ */
