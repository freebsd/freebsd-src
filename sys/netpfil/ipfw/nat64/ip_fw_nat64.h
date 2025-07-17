/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2015-2019 Yandex LLC
 * Copyright (c) 2015-2019 Andrey V. Elsukov <ae@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef	_IP_FW_NAT64_H_
#define	_IP_FW_NAT64_H_

#define	DPRINTF(mask, fmt, ...)	\
    if (V_nat64_debug & (mask))	\
	printf("NAT64: %s: " fmt "\n", __func__, ## __VA_ARGS__)
#define	DP_GENERIC	0x0001
#define	DP_OBJ		0x0002
#define	DP_JQUEUE	0x0004
#define	DP_STATE	0x0008
#define	DP_DROPS	0x0010
#define	DP_ALL		0xFFFF

VNET_DECLARE(int, nat64_debug);
#define	V_nat64_debug		VNET(nat64_debug)

#if 0
#define	NAT64NOINLINE	__noinline
#else
#define	NAT64NOINLINE
#endif

int	nat64stl_init(struct ip_fw_chain *ch, int first);
void	nat64stl_uninit(struct ip_fw_chain *ch, int last);
int	nat64lsn_init(struct ip_fw_chain *ch, int first);
void	nat64lsn_uninit(struct ip_fw_chain *ch, int last);
int	nat64clat_init(struct ip_fw_chain *ch, int first);
void	nat64clat_uninit(struct ip_fw_chain *ch, int last);

#define	NAT64_DEFINE_OPCODE_REWRITER(mod, name, ops)			\
static int								\
mod ## _classify(ipfw_insn *cmd0, uint32_t *puidx, uint8_t *ptype)	\
{									\
	ipfw_insn *icmd;						\
	icmd = cmd0 - F_LEN(cmd0);					\
	if (icmd->opcode != O_EXTERNAL_ACTION ||			\
	    insntod(icmd, kidx)->kidx != V_ ## mod ## _eid)		\
		return (1);						\
	*puidx = insntod(cmd0, kidx)->kidx;				\
	*ptype = 0;							\
	return (0);							\
}									\
static void								\
mod ## _update_kidx(ipfw_insn *cmd0, uint32_t idx)			\
{									\
	insntod(cmd0, kidx)->kidx = idx;				\
}									\
static int								\
mod ## _findbyname(struct ip_fw_chain *ch, struct tid_info *ti,		\
    struct named_object **pno)						\
{									\
	return (ipfw_objhash_find_type(CHAIN_TO_SRV(ch), ti,		\
	    IPFW_TLV_## name ## _NAME, pno));				\
}									\
static struct named_object *						\
mod ## _findbykidx(struct ip_fw_chain *ch, uint32_t idx)		\
{									\
	struct namedobj_instance *ni;					\
	struct named_object *no;					\
	IPFW_UH_WLOCK_ASSERT(ch);					\
	ni = CHAIN_TO_SRV(ch);						\
	no = ipfw_objhash_lookup_kidx(ni, idx);				\
	KASSERT(no != NULL, ("NAT with index %u not found", idx));	\
	return (no);							\
}									\
static struct opcode_obj_rewrite ops[] = {				\
	{								\
		.opcode = O_EXTERNAL_INSTANCE,				\
		.etlv = IPFW_TLV_EACTION /* just show it isn't table */,\
		.classifier = mod ## _classify,				\
		.update = mod ## _update_kidx,				\
		.find_byname = mod ## _findbyname,			\
		.find_bykidx = mod ## _findbykidx,			\
		.manage_sets = mod ## _manage_sets,			\
	},								\
}

#endif /* _IP_FW_NAT64_H_ */
