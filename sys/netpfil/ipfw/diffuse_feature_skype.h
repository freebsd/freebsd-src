/*-
 * Copyright (c) 2010-2011
 * 	Swinburne University of Technology, Melbourne, Australia.
 * All rights reserved.
 *
 * This software was developed at the Centre for Advanced Internet
 * Architectures, Swinburne University of Technology, by Rozanna Jesudasan.
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

/*
 * DIFFUSE Skype feature.
 */

#ifndef _NETINET_IPFW_DIFFUSE_FEATURE_SKYPE_H_
#define _NETINET_IPFW_DIFFUSE_FEATURE_SKYPE_H_

#define	DI_DEFAULT_SKYPE_WINDOW 100

#define	di_feature_skype_config di_feature_plen_config

/* Main properties, used in kernel and userspace. */
#define	DI_SKYPE_NAME "skype"
#define	DI_SKYPE_TYPE DI_FEATURE_ALG_BIDIRECTIONAL
#define	DI_SKYPE_NO_STATS 3
#define	DI_SKYPE_STAT_NAMES_DECL char *di_skype_stat_names[DI_SKYPE_NO_STATS]
#define	DI_SKYPE_STAT_NAMES DI_SKYPE_STAT_NAMES_DECL =			\
{									\
	 "mean",							\
	 "atpd",							\
	 "ratio"							\
};

struct di_feature_module * skype_module(void);

#endif /* _NETINET_IPFW_DIFFUSE_FEATURE_SKYPE_H_ */
