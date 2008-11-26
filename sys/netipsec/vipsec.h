/*
 * Copyright (c) 2007-2008 University of Zagreb
 * Copyright (c) 2007-2008 FreeBSD Foundation
 *
 * This software was developed by the University of Zagreb and the
 * FreeBSD Foundation under sponsorship by the Stichting NLnet and the
 * FreeBSD Foundation.
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
 * $FreeBSD$
 */

#ifndef _NETIPSEC_VIPSEC_H_
#define _NETIPSEC_VIPSEC_H_

#ifdef VIMAGE
#include <sys/proc.h>
#include <sys/protosw.h>
#include <sys/socket.h>

#include <netipsec/ipsec.h>
#include <netipsec/esp_var.h>
#include <netipsec/ah_var.h>
#include <netipsec/ipcomp_var.h>
#include <netipsec/ipip_var.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/route.h>
#include <net/raw_cb.h>

#include <netipsec/keysock.h>

struct vnet_ipsec {
	int			_ipsec_debug;
	struct	ipsecstat	_ipsec4stat;
	struct	secpolicy 	_ip4_def_policy;

	int			_ip4_esp_trans_deflev;
	int			_ip4_esp_net_deflev;
	int			_ip4_ah_trans_deflev;
	int			_ip4_ah_net_deflev;
	int			_ip4_ah_offsetmask;
	int			_ip4_ipsec_dfbit;
	int			_ip4_ipsec_ecn;
	int			_ip4_esp_randpad;

	int			_ipsec_replay;
	int			_ipsec_integrity;
	int			_crypto_support;

	u_int32_t		_key_debug_level;	
	u_int			_key_spi_trycnt;
	u_int32_t		_key_spi_minval;
	u_int32_t		_key_spi_maxval;  
	u_int32_t		_policy_id;
	u_int			_key_int_random;   
	u_int			_key_larval_lifetime; 
	int			_key_blockacq_count;
	int			_key_blockacq_lifetime;
	int			_key_preferred_oldsa;
	u_int32_t		_acq_seq;

	int			_esp_enable;
	struct espstat		_espstat;
	int			_esp_max_ivlen;
	int			_ipsec_esp_keymin;
	int			_ipsec_esp_auth;
	int			_ipsec_ah_keymin;
	int			_ipip_allow;
	struct ipipstat		_ipipstat;

	struct ipsecstat	_ipsec6stat;
	int			_ip6_esp_trans_deflev;
	int			_ip6_esp_net_deflev;
	int			_ip6_ah_trans_deflev;
	int			_ip6_ah_net_deflev;
	int			_ip6_ipsec_ecn;

	int			_ah_enable;
	int			_ah_cleartos;
	struct ahstat		_ahstat;

	int			_ipcomp_enable;
	struct ipcompstat	_ipcompstat;

	struct pfkeystat	_pfkeystat;
	struct key_cb		_key_cb;
	LIST_HEAD(, secpolicy)	_sptree[IPSEC_DIR_MAX];
	LIST_HEAD(, secashead)	_sahtree;
	LIST_HEAD(, secreg)	_regtree[SADB_SATYPE_MAX + 1];
	LIST_HEAD(, secacq)	_acqtree;
	LIST_HEAD(, secspacq)	_spacqtree;
};
#endif

/*
 * Symbol translation macros
 */
#define	INIT_VNET_IPSEC(vnet) \
	INIT_FROM_VNET(vnet, VNET_MOD_IPSEC, struct vnet_ipsec, vnet_ipsec)

#define	VNET_IPSEC(sym)	VSYM(vnet_ipsec, sym)

#define	V_acq_seq			VNET_IPSEC(acq_seq)
#define	V_acqtree			VNET_IPSEC(acqtree)
#define	V_ah_cleartos			VNET_IPSEC(ah_cleartos)
#define	V_ah_enable			VNET_IPSEC(ah_enable)
#define	V_ahstat			VNET_IPSEC(ahstat)
#define	V_crypto_support		VNET_IPSEC(crypto_support)
#define	V_esp_enable			VNET_IPSEC(esp_enable)
#define	V_esp_max_ivlen			VNET_IPSEC(esp_max_ivlen)
#define	V_espstat			VNET_IPSEC(espstat)
#define	V_ip4_ah_net_deflev		VNET_IPSEC(ip4_ah_net_deflev)
#define	V_ip4_ah_offsetmask		VNET_IPSEC(ip4_ah_offsetmask)
#define	V_ip4_ah_trans_deflev		VNET_IPSEC(ip4_ah_trans_deflev)
#define	V_ip4_def_policy		VNET_IPSEC(ip4_def_policy)
#define	V_ip4_esp_net_deflev		VNET_IPSEC(ip4_esp_net_deflev)
#define	V_ip4_esp_randpad		VNET_IPSEC(ip4_esp_randpad)
#define	V_ip4_esp_trans_deflev		VNET_IPSEC(ip4_esp_trans_deflev)
#define	V_ip4_ipsec_dfbit		VNET_IPSEC(ip4_ipsec_dfbit)
#define	V_ip4_ipsec_ecn			VNET_IPSEC(ip4_ipsec_ecn)
#define	V_ip6_ah_net_deflev		VNET_IPSEC(ip6_ah_net_deflev)
#define	V_ip6_ah_trans_deflev		VNET_IPSEC(ip6_ah_trans_deflev)
#define	V_ip6_esp_net_deflev		VNET_IPSEC(ip6_esp_net_deflev)
#define	V_ip6_esp_randpad		VNET_IPSEC(ip6_esp_randpad)
#define	V_ip6_esp_trans_deflev		VNET_IPSEC(ip6_esp_trans_deflev)
#define	V_ip6_ipsec_ecn			VNET_IPSEC(ip6_ipsec_ecn)
#define	V_ipcomp_enable			VNET_IPSEC(ipcomp_enable)
#define	V_ipcompstat			VNET_IPSEC(ipcompstat)
#define	V_ipip_allow			VNET_IPSEC(ipip_allow)
#define	V_ipipstat			VNET_IPSEC(ipipstat)
#define	V_ipsec4stat			VNET_IPSEC(ipsec4stat)
#define	V_ipsec6stat			VNET_IPSEC(ipsec6stat)
#define	V_ipsec_ah_keymin		VNET_IPSEC(ipsec_ah_keymin)
#define	V_ipsec_debug			VNET_IPSEC(ipsec_debug)
#define	V_ipsec_esp_auth		VNET_IPSEC(ipsec_esp_auth)
#define	V_ipsec_esp_keymin		VNET_IPSEC(ipsec_esp_keymin)
#define	V_ipsec_integrity		VNET_IPSEC(ipsec_integrity)
#define	V_ipsec_replay			VNET_IPSEC(ipsec_replay)
#define	V_key_blockacq_count		VNET_IPSEC(key_blockacq_count)
#define	V_key_blockacq_lifetime		VNET_IPSEC(key_blockacq_lifetime)
#define	V_key_cb			VNET_IPSEC(key_cb)
#define	V_key_debug_level		VNET_IPSEC(key_debug_level)
#define	V_key_int_random		VNET_IPSEC(key_int_random)
#define	V_key_larval_lifetime		VNET_IPSEC(key_larval_lifetime)
#define	V_key_preferred_oldsa		VNET_IPSEC(key_preferred_oldsa)
#define	V_key_spi_maxval		VNET_IPSEC(key_spi_maxval)	
#define	V_key_spi_minval		VNET_IPSEC(key_spi_minval)
#define	V_key_spi_trycnt		VNET_IPSEC(key_spi_trycnt)
#define	V_pfkeystat			VNET_IPSEC(pfkeystat)
#define	V_policy_id			VNET_IPSEC(policy_id)
#define	V_regtree			VNET_IPSEC(regtree)
#define	V_sahtree			VNET_IPSEC(sahtree)
#define	V_spacqtree			VNET_IPSEC(spacqtree)
#define	V_sptree			VNET_IPSEC(sptree)

#endif /* !_NETIPSEC_VIPSEC_H_ */
