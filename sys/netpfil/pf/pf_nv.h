/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2021 Rubicon Communications, LLC (Netgate)
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
 */
#ifndef _PF_NV_H_
#define _PF_NV_H_

#include <sys/nv.h>
#include <sys/sdt.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/pfvar.h>

SDT_PROBE_DECLARE(pf, ioctl, function, error);
SDT_PROBE_DECLARE(pf, ioctl, nvchk, error);

#define	ERROUT_FUNCTION(target, x)					\
	do {								\
		error = (x);						\
		SDT_PROBE3(pf, ioctl, function, error, __func__, error,	\
		    __LINE__);						\
		goto target;						\
	} while (0)

#define	PFNV_CHK(x)	do {	\
	error = (x);		\
	SDT_PROBE2(pf, ioctl, nvchk, error, error, __LINE__);	\
	if (error != 0)		\
		goto errout;	\
	} while (0)

#define PF_NV_DEF_UINT(fnname, type, max)				\
	int pf_nv ## fnname ## _opt(const nvlist_t *, const char *,	\
	    type *, type);						\
	int pf_nv ## fnname(const nvlist_t *, const char *, type *);	\
	int pf_nv ## fnname ## _array(const nvlist_t *, const char *,	\
	    type *,size_t, size_t *);					\
	void pf_ ## fnname ## _array_nv(nvlist_t *, const char *,	\
	    const type *, size_t);

PF_NV_DEF_UINT(uint8, uint8_t, UINT8_MAX);
PF_NV_DEF_UINT(uint16, uint16_t, UINT16_MAX);
PF_NV_DEF_UINT(uint32, uint32_t, UINT32_MAX);
PF_NV_DEF_UINT(uint64, uint64_t, UINT64_MAX);

int	pf_nvbool(const nvlist_t *, const char *, bool *);
int	pf_nvbinary(const nvlist_t *, const char *, void *, size_t);
int	pf_nvint(const nvlist_t *, const char *, int *);
int	pf_nvstring(const nvlist_t *, const char *, char *, size_t);

/* Translation functions */

int		 pf_check_rule_addr(const struct pf_rule_addr *);

nvlist_t	*pf_krule_to_nvrule(struct pf_krule *);
int		 pf_nvrule_to_krule(const nvlist_t *, struct pf_krule *);
int		 pf_nvstate_kill_to_kstate_kill(const nvlist_t *,
		    struct pf_kstate_kill *);
nvlist_t	*pf_state_to_nvstate(const struct pf_kstate *);

nvlist_t	*pf_keth_rule_to_nveth_rule(const struct pf_keth_rule *);
int		 pf_nveth_rule_to_keth_rule(const nvlist_t *, struct pf_keth_rule *);
#endif
