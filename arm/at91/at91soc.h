/*-
 * Copyright (c) 2012 Warner Losh.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* $FreeBSD$ */

#ifndef _ARM_AT91_AT91SOC_H_
#define _ARM_AT91_AT91SOC_H_

#include <sys/linker_set.h>

struct at91_soc {
	enum at91_soc_type	soc_type;	/* Family of mail type of SoC */
	enum at91_soc_subtype 	soc_subtype;	/* More specific soc, if any */
	struct at91_soc_data	*soc_data;
};
 
// Make varadic
#define AT91_SOC(type, data)			\
	static struct at91_soc this_soc = {	\
		.soc_type = type,		\
		.soc_subtype = AT91_ST_ANY,	\
		.soc_data = data,		\
	};					\
	DATA_SET(at91_socs, this_soc);

#define AT91_SOC_SUB(type, subtype, data)	\
	static struct at91_soc this_soc = {	\
		.soc_type = type,		\
		.soc_subtype = subtype,		\
		.soc_data = data,		\
	};					\
	DATA_SET(at91_socs, this_soc);

struct at91_soc_data *at91_match_soc(enum at91_soc_type, enum at91_soc_subtype);

#endif /* _ARM_AT91_AT91SOC_H_ */
