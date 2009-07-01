/*-
 * Copyright (c) 2008-2009
 * 	Swinburne University of Technology, Melbourne, Australia
 * Copyright (c) 2009 Lawrence Stewart <lstewart@freebsd.org>
 * All rights reserved.
 *
 * This software was developed at the Centre for Advanced Internet
 * Architectures, Swinburne University, by Lawrence Stewart and James Healy,
 * made possible in part by a grant from the Cisco University Research Program
 * Fund at Community Foundation Silicon Valley.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _NETINET_CC_MODULE_H_
#define _NETINET_CC_MODULE_H_

#define	DECLARE_CC_MODULE(ccname, ccalgo) 				\
	static moduledata_t cc_##ccname = {				\
		#ccname,						\
		cc_modevent,						\
		ccalgo							\
	};								\
	DECLARE_MODULE(ccname, cc_##ccname, SI_SUB_PROTO_IFATTACHDOMAIN, \
	    SI_ORDER_ANY)

int	cc_modevent(module_t mod, int type, void *data);

#endif /* _NETINET_CC_MODULE_H_ */
