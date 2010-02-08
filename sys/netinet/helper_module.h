/*-
 * Copyright (c) 2010 Lawrence Stewart <lstewart@freebsd.org>
 * All rights reserved.
 *
 * This software was developed at the Centre for Advanced Internet
 * Architectures, Swinburne University, by Lawrence Stewart,
 * made possible in part by a grant from the FreeBSD Foundation.
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

#ifndef _NETINET_HELPER_MODULE_H_
#define _NETINET_HELPER_MODULE_H_

struct helper_modevent_data {
	char *name;
	struct helper *helper;
	int uma_zsize;
	uma_ctor umactor;
	uma_dtor umadtor;
};

#define	DECLARE_HELPER(hname, hdata) 					\
	static struct helper_modevent_data hmd_##hname = {		\
		.name = #hname,						\
		.helper = hdata						\
	};								\
	static moduledata_t h_##hname = {				\
		.name = #hname,						\
		.evhand = helper_modevent,				\
		.priv = &hmd_##hname					\
	};								\
	DECLARE_MODULE(hname, h_##hname, SI_SUB_PROTO_IFATTACHDOMAIN, \
	    SI_ORDER_ANY)

#define	DECLARE_HELPER_UMA(hname, hdata, size, ctor, dtor)		\
	static struct helper_modevent_data hmd_##hname = {		\
		.name = #hname,						\
		.helper = hdata,					\
		.uma_zsize = size,					\
		.umactor = ctor,					\
		.umadtor = dtor						\
	};								\
	static moduledata_t h_##hname = {				\
		.name = #hname,						\
		.evhand = helper_modevent,				\
		.priv = &hmd_##hname					\
	};								\
	DECLARE_MODULE(hname, h_##hname, SI_SUB_PROTO_IFATTACHDOMAIN, \
	    SI_ORDER_ANY)

int	helper_modevent(module_t mod, int type, void *data);

MALLOC_DECLARE(M_HELPER);
MALLOC_DEFINE(M_HELPER, "helper data", "Blah");


#endif /* _NETINET_HELPER_MODULE_H_ */
