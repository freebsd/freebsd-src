/*-
 * Copyright (c) 2007-2015 Solarflare Communications Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are
 * those of the authors and should not be interpreted as representing official
 * policies, either expressed or implied, of the FreeBSD Project.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "efsys.h"
#include "efx.h"
#include "efx_types.h"
#include "efx_regs.h"
#include "efx_impl.h"


#if EFSYS_OPT_FILTER

#if EFSYS_OPT_FALCON || EFSYS_OPT_SIENA

static	__checkReturn	efx_rc_t
falconsiena_filter_init(
	__in		efx_nic_t *enp);

static			void
falconsiena_filter_fini(
	__in		efx_nic_t *enp);

static	__checkReturn	efx_rc_t
falconsiena_filter_restore(
	__in		efx_nic_t *enp);

static	__checkReturn	efx_rc_t
falconsiena_filter_add(
	__in		efx_nic_t *enp,
	__inout		efx_filter_spec_t *spec,
	__in		boolean_t may_replace);

static	__checkReturn	efx_rc_t
falconsiena_filter_delete(
	__in		efx_nic_t *enp,
	__inout		efx_filter_spec_t *spec);

static	__checkReturn	efx_rc_t
falconsiena_filter_supported_filters(
	__in		efx_nic_t *enp,
	__out		uint32_t *list,
	__out		size_t *length);

#endif /* EFSYS_OPT_FALCON || EFSYS_OPT_SIENA */

#if EFSYS_OPT_FALCON
static efx_filter_ops_t	__efx_filter_falcon_ops = {
	falconsiena_filter_init,		/* efo_init */
	falconsiena_filter_fini,		/* efo_fini */
	falconsiena_filter_restore,		/* efo_restore */
	falconsiena_filter_add,			/* efo_add */
	falconsiena_filter_delete,		/* efo_delete */
	falconsiena_filter_supported_filters,	/* efo_supported_filters */
	NULL,					/* efo_reconfigure */
};
#endif /* EFSYS_OPT_FALCON */

#if EFSYS_OPT_SIENA
static efx_filter_ops_t	__efx_filter_siena_ops = {
	falconsiena_filter_init,		/* efo_init */
	falconsiena_filter_fini,		/* efo_fini */
	falconsiena_filter_restore,		/* efo_restore */
	falconsiena_filter_add,			/* efo_add */
	falconsiena_filter_delete,		/* efo_delete */
	falconsiena_filter_supported_filters,	/* efo_supported_filters */
	NULL,					/* efo_reconfigure */
};
#endif /* EFSYS_OPT_SIENA */

#if EFSYS_OPT_HUNTINGTON
static efx_filter_ops_t	__efx_filter_hunt_ops = {
	hunt_filter_init,		/* efo_init */
	hunt_filter_fini,		/* efo_fini */
	hunt_filter_restore,		/* efo_restore */
	hunt_filter_add,		/* efo_add */
	hunt_filter_delete,		/* efo_delete */
	hunt_filter_supported_filters,	/* efo_supported_filters */
	hunt_filter_reconfigure,	/* efo_reconfigure */
};
#endif /* EFSYS_OPT_HUNTINGTON */

	__checkReturn	efx_rc_t
efx_filter_insert(
	__in		efx_nic_t *enp,
	__inout		efx_filter_spec_t *spec)
{
	efx_filter_ops_t *efop = enp->en_efop;

	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_FILTER);
	EFSYS_ASSERT3P(spec, !=, NULL);
	EFSYS_ASSERT3U(spec->efs_flags, &, EFX_FILTER_FLAG_RX);

	return (efop->efo_add(enp, spec, B_FALSE));
}

	__checkReturn	efx_rc_t
efx_filter_remove(
	__in		efx_nic_t *enp,
	__inout		efx_filter_spec_t *spec)
{
	efx_filter_ops_t *efop = enp->en_efop;

	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_FILTER);
	EFSYS_ASSERT3P(spec, !=, NULL);
	EFSYS_ASSERT3U(spec->efs_flags, &, EFX_FILTER_FLAG_RX);

#if EFSYS_OPT_RX_SCALE
	spec->efs_rss_context = enp->en_rss_context;
#endif

	return (efop->efo_delete(enp, spec));
}

	__checkReturn	efx_rc_t
efx_filter_restore(
	__in		efx_nic_t *enp)
{
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_FILTER);

	if ((rc = enp->en_efop->efo_restore(enp)) != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn	efx_rc_t
efx_filter_init(
	__in		efx_nic_t *enp)
{
	efx_filter_ops_t *efop;
	efx_rc_t rc;

	/* Check that efx_filter_spec_t is 64 bytes. */
	EFX_STATIC_ASSERT(sizeof (efx_filter_spec_t) == 64);

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PROBE);
	EFSYS_ASSERT(!(enp->en_mod_flags & EFX_MOD_FILTER));

	switch (enp->en_family) {
#if EFSYS_OPT_FALCON
	case EFX_FAMILY_FALCON:
		efop = (efx_filter_ops_t *)&__efx_filter_falcon_ops;
		break;
#endif /* EFSYS_OPT_FALCON */

#if EFSYS_OPT_SIENA
	case EFX_FAMILY_SIENA:
		efop = (efx_filter_ops_t *)&__efx_filter_siena_ops;
		break;
#endif /* EFSYS_OPT_SIENA */

#if EFSYS_OPT_HUNTINGTON
	case EFX_FAMILY_HUNTINGTON:
		efop = (efx_filter_ops_t *)&__efx_filter_hunt_ops;
		break;
#endif /* EFSYS_OPT_HUNTINGTON */

	default:
		EFSYS_ASSERT(0);
		rc = ENOTSUP;
		goto fail1;
	}

	if ((rc = efop->efo_init(enp)) != 0)
		goto fail2;

	enp->en_efop = efop;
	enp->en_mod_flags |= EFX_MOD_FILTER;
	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	enp->en_efop = NULL;
	enp->en_mod_flags &= ~EFX_MOD_FILTER;
	return (rc);
}

			void
efx_filter_fini(
	__in		efx_nic_t *enp)
{
	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PROBE);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_FILTER);

	enp->en_efop->efo_fini(enp);

	enp->en_efop = NULL;
	enp->en_mod_flags &= ~EFX_MOD_FILTER;
}

	__checkReturn	efx_rc_t
efx_filter_supported_filters(
	__in		efx_nic_t *enp,
	__out		uint32_t *list,
	__out		size_t *length)
{
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PROBE);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_FILTER);
	EFSYS_ASSERT(enp->en_efop->efo_supported_filters != NULL);

	if ((rc = enp->en_efop->efo_supported_filters(enp, list, length)) != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn	efx_rc_t
efx_filter_reconfigure(
	__in				efx_nic_t *enp,
	__in_ecount(6)			uint8_t const *mac_addr,
	__in				boolean_t all_unicst,
	__in				boolean_t mulcst,
	__in				boolean_t all_mulcst,
	__in				boolean_t brdcst,
	__in_ecount(6*count)		uint8_t const *addrs,
	__in				int count)
{
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PROBE);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_FILTER);

	if (enp->en_efop->efo_reconfigure != NULL) {
		if ((rc = enp->en_efop->efo_reconfigure(enp, mac_addr,
							all_unicst, mulcst,
							all_mulcst, brdcst,
							addrs, count)) != 0)
			goto fail1;
	}

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

		void
efx_filter_spec_init_rx(
	__inout		efx_filter_spec_t *spec,
	__in		efx_filter_priority_t priority,
	__in		efx_filter_flag_t flags,
	__in		efx_rxq_t *erp)
{
	EFSYS_ASSERT3P(spec, !=, NULL);
	EFSYS_ASSERT3P(erp, !=, NULL);
	EFSYS_ASSERT((flags & ~(EFX_FILTER_FLAG_RX_RSS |
				EFX_FILTER_FLAG_RX_SCATTER)) == 0);

	memset(spec, 0, sizeof (*spec));
	spec->efs_priority = priority;
	spec->efs_flags = EFX_FILTER_FLAG_RX | flags;
	spec->efs_rss_context = EFX_FILTER_SPEC_RSS_CONTEXT_DEFAULT;
	spec->efs_dmaq_id = (uint16_t)erp->er_index;
}

		void
efx_filter_spec_init_tx(
	__inout		efx_filter_spec_t *spec,
	__in		efx_txq_t *etp)
{
	EFSYS_ASSERT3P(spec, !=, NULL);
	EFSYS_ASSERT3P(etp, !=, NULL);

	memset(spec, 0, sizeof (*spec));
	spec->efs_priority = EFX_FILTER_PRI_REQUIRED;
	spec->efs_flags = EFX_FILTER_FLAG_TX;
	spec->efs_dmaq_id = (uint16_t)etp->et_index;
}


/*
 *  Specify IPv4 host, transport protocol and port in a filter specification
 */
__checkReturn		efx_rc_t
efx_filter_spec_set_ipv4_local(
	__inout		efx_filter_spec_t *spec,
	__in		uint8_t proto,
	__in		uint32_t host,
	__in		uint16_t port)
{
	EFSYS_ASSERT3P(spec, !=, NULL);

	spec->efs_match_flags |=
		EFX_FILTER_MATCH_ETHER_TYPE | EFX_FILTER_MATCH_IP_PROTO |
		EFX_FILTER_MATCH_LOC_HOST | EFX_FILTER_MATCH_LOC_PORT;
	spec->efs_ether_type = EFX_ETHER_TYPE_IPV4;
	spec->efs_ip_proto = proto;
	spec->efs_loc_host.eo_u32[0] = host;
	spec->efs_loc_port = port;
	return (0);
}

/*
 * Specify IPv4 hosts, transport protocol and ports in a filter specification
 */
__checkReturn		efx_rc_t
efx_filter_spec_set_ipv4_full(
	__inout		efx_filter_spec_t *spec,
	__in		uint8_t proto,
	__in		uint32_t lhost,
	__in		uint16_t lport,
	__in		uint32_t rhost,
	__in		uint16_t rport)
{
	EFSYS_ASSERT3P(spec, !=, NULL);

	spec->efs_match_flags |=
		EFX_FILTER_MATCH_ETHER_TYPE | EFX_FILTER_MATCH_IP_PROTO |
		EFX_FILTER_MATCH_LOC_HOST | EFX_FILTER_MATCH_LOC_PORT |
		EFX_FILTER_MATCH_REM_HOST | EFX_FILTER_MATCH_REM_PORT;
	spec->efs_ether_type = EFX_ETHER_TYPE_IPV4;
	spec->efs_ip_proto = proto;
	spec->efs_loc_host.eo_u32[0] = lhost;
	spec->efs_loc_port = lport;
	spec->efs_rem_host.eo_u32[0] = rhost;
	spec->efs_rem_port = rport;
	return (0);
}

/*
 * Specify local Ethernet address and/or VID in filter specification
 */
__checkReturn		efx_rc_t
efx_filter_spec_set_eth_local(
	__inout		efx_filter_spec_t *spec,
	__in		uint16_t vid,
	__in		const uint8_t *addr)
{
	EFSYS_ASSERT3P(spec, !=, NULL);
	EFSYS_ASSERT3P(addr, !=, NULL);

	if (vid == EFX_FILTER_SPEC_VID_UNSPEC && addr == NULL)
		return (EINVAL);

	if (vid != EFX_FILTER_SPEC_VID_UNSPEC) {
		spec->efs_match_flags |= EFX_FILTER_MATCH_OUTER_VID;
		spec->efs_outer_vid = vid;
	}
	if (addr != NULL) {
		spec->efs_match_flags |= EFX_FILTER_MATCH_LOC_MAC;
		memcpy(spec->efs_loc_mac, addr, EFX_MAC_ADDR_LEN);
	}
	return (0);
}

/*
 * Specify matching otherwise-unmatched unicast in a filter specification
 */
__checkReturn		efx_rc_t
efx_filter_spec_set_uc_def(
	__inout		efx_filter_spec_t *spec)
{
	EFSYS_ASSERT3P(spec, !=, NULL);

	spec->efs_match_flags |= EFX_FILTER_MATCH_LOC_MAC_IG;
	return (0);
}

/*
 * Specify matching otherwise-unmatched multicast in a filter specification
 */
__checkReturn		efx_rc_t
efx_filter_spec_set_mc_def(
	__inout		efx_filter_spec_t *spec)
{
	EFSYS_ASSERT3P(spec, !=, NULL);

	spec->efs_match_flags |= EFX_FILTER_MATCH_LOC_MAC_IG;
	spec->efs_loc_mac[0] = 1;
	return (0);
}



#if EFSYS_OPT_FALCON || EFSYS_OPT_SIENA

/*
 * "Fudge factors" - difference between programmed value and actual depth.
 * Due to pipelined implementation we need to program H/W with a value that
 * is larger than the hop limit we want.
 */
#define	FILTER_CTL_SRCH_FUDGE_WILD 3
#define	FILTER_CTL_SRCH_FUDGE_FULL 1

/*
 * Hard maximum hop limit.  Hardware will time-out beyond 200-something.
 * We also need to avoid infinite loops in efx_filter_search() when the
 * table is full.
 */
#define	FILTER_CTL_SRCH_MAX 200

static	__checkReturn	efx_rc_t
falconsiena_filter_spec_from_gen_spec(
	__out		falconsiena_filter_spec_t *fs_spec,
	__in		efx_filter_spec_t *gen_spec)
{
	efx_rc_t rc;
	boolean_t is_full = B_FALSE;

	if (gen_spec->efs_flags & EFX_FILTER_FLAG_TX)
		EFSYS_ASSERT3U(gen_spec->efs_flags, ==, EFX_FILTER_FLAG_TX);
	else
		EFSYS_ASSERT3U(gen_spec->efs_flags, &, EFX_FILTER_FLAG_RX);

	/* Falconsiena only has one RSS context */
	if ((gen_spec->efs_flags & EFX_FILTER_FLAG_RX_RSS) &&
	    gen_spec->efs_rss_context != 0) {
		rc = EINVAL;
		goto fail1;
	}

	fs_spec->fsfs_flags = gen_spec->efs_flags;
	fs_spec->fsfs_dmaq_id = gen_spec->efs_dmaq_id;

	switch (gen_spec->efs_match_flags) {
	case EFX_FILTER_MATCH_ETHER_TYPE | EFX_FILTER_MATCH_IP_PROTO |
	    EFX_FILTER_MATCH_LOC_HOST | EFX_FILTER_MATCH_LOC_PORT |
	    EFX_FILTER_MATCH_REM_HOST | EFX_FILTER_MATCH_REM_PORT:
		is_full = B_TRUE;
		/* Fall through */
	case EFX_FILTER_MATCH_ETHER_TYPE | EFX_FILTER_MATCH_IP_PROTO |
	    EFX_FILTER_MATCH_LOC_HOST | EFX_FILTER_MATCH_LOC_PORT: {
		uint32_t rhost, host1, host2;
		uint16_t rport, port1, port2;

		if (gen_spec->efs_ether_type != EFX_ETHER_TYPE_IPV4) {
			rc = ENOTSUP;
			goto fail2;
		}
		if (gen_spec->efs_loc_port == 0 ||
		    (is_full && gen_spec->efs_rem_port == 0)) {
			rc = EINVAL;
			goto fail3;
		}
		switch (gen_spec->efs_ip_proto) {
		case EFX_IPPROTO_TCP:
			if (gen_spec->efs_flags & EFX_FILTER_FLAG_TX) {
				fs_spec->fsfs_type = (is_full ?
				    EFX_FS_FILTER_TX_TCP_FULL :
				    EFX_FS_FILTER_TX_TCP_WILD);
			} else {
				fs_spec->fsfs_type = (is_full ?
				    EFX_FS_FILTER_RX_TCP_FULL :
				    EFX_FS_FILTER_RX_TCP_WILD);
			}
			break;
		case EFX_IPPROTO_UDP:
			if (gen_spec->efs_flags & EFX_FILTER_FLAG_TX) {
				fs_spec->fsfs_type = (is_full ?
				    EFX_FS_FILTER_TX_UDP_FULL :
				    EFX_FS_FILTER_TX_UDP_WILD);
			} else {
				fs_spec->fsfs_type = (is_full ?
				    EFX_FS_FILTER_RX_UDP_FULL :
				    EFX_FS_FILTER_RX_UDP_WILD);
			}
			break;
		default:
			rc = ENOTSUP;
			goto fail4;
		}
		/*
		 * The filter is constructed in terms of source and destination,
		 * with the odd wrinkle that the ports are swapped in a UDP
		 * wildcard filter. We need to convert from local and remote
		 * addresses (zero for a wildcard).
		 */
		rhost = is_full ? gen_spec->efs_rem_host.eo_u32[0] : 0;
		rport = is_full ? gen_spec->efs_rem_port : 0;
		if (gen_spec->efs_flags & EFX_FILTER_FLAG_TX) {
			host1 = gen_spec->efs_loc_host.eo_u32[0];
			host2 = rhost;
		} else {
			host1 = rhost;
			host2 = gen_spec->efs_loc_host.eo_u32[0];
		}
		if (gen_spec->efs_flags & EFX_FILTER_FLAG_TX) {
			if (fs_spec->fsfs_type == EFX_FS_FILTER_TX_UDP_WILD) {
				port1 = rport;
				port2 = gen_spec->efs_loc_port;
			} else {
				port1 = gen_spec->efs_loc_port;
				port2 = rport;
			}
		} else {
			if (fs_spec->fsfs_type == EFX_FS_FILTER_RX_UDP_WILD) {
				port1 = gen_spec->efs_loc_port;
				port2 = rport;
			} else {
				port1 = rport;
				port2 = gen_spec->efs_loc_port;
			}
		}
		fs_spec->fsfs_dword[0] = (host1 << 16) | port1;
		fs_spec->fsfs_dword[1] = (port2 << 16) | (host1 >> 16);
		fs_spec->fsfs_dword[2] = host2;
		break;
	}

	case EFX_FILTER_MATCH_LOC_MAC | EFX_FILTER_MATCH_OUTER_VID:
		is_full = B_TRUE;
		/* Fall through */
	case EFX_FILTER_MATCH_LOC_MAC:
		if (gen_spec->efs_flags & EFX_FILTER_FLAG_TX) {
			fs_spec->fsfs_type = (is_full ?
			    EFX_FS_FILTER_TX_MAC_FULL :
			    EFX_FS_FILTER_TX_MAC_WILD);
		} else {
			fs_spec->fsfs_type = (is_full ?
			    EFX_FS_FILTER_RX_MAC_FULL :
			    EFX_FS_FILTER_RX_MAC_WILD);
		}
		fs_spec->fsfs_dword[0] = is_full ? gen_spec->efs_outer_vid : 0;
		fs_spec->fsfs_dword[1] =
		    gen_spec->efs_loc_mac[2] << 24 |
		    gen_spec->efs_loc_mac[3] << 16 |
		    gen_spec->efs_loc_mac[4] <<  8 |
		    gen_spec->efs_loc_mac[5];
		fs_spec->fsfs_dword[2] =
		    gen_spec->efs_loc_mac[0] << 8 |
		    gen_spec->efs_loc_mac[1];
		break;

	default:
		EFSYS_ASSERT(B_FALSE);
		rc = ENOTSUP;
		goto fail5;
	}

	return (0);

fail5:
	EFSYS_PROBE(fail5);
fail4:
	EFSYS_PROBE(fail4);
fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

/*
 * The filter hash function is LFSR polynomial x^16 + x^3 + 1 of a 32-bit
 * key derived from the n-tuple.
 */
static			uint16_t
falconsiena_filter_tbl_hash(
	__in		uint32_t key)
{
	uint16_t tmp;

	/* First 16 rounds */
	tmp = 0x1fff ^ (uint16_t)(key >> 16);
	tmp = tmp ^ tmp >> 3 ^ tmp >> 6;
	tmp = tmp ^ tmp >> 9;

	/* Last 16 rounds */
	tmp = tmp ^ tmp << 13 ^ (uint16_t)(key & 0xffff);
	tmp = tmp ^ tmp >> 3 ^ tmp >> 6;
	tmp = tmp ^ tmp >> 9;

	return (tmp);
}

/*
 * To allow for hash collisions, filter search continues at these
 * increments from the first possible entry selected by the hash.
 */
static			uint16_t
falconsiena_filter_tbl_increment(
	__in		uint32_t key)
{
	return ((uint16_t)(key * 2 - 1));
}

static	__checkReturn	boolean_t
falconsiena_filter_test_used(
	__in		falconsiena_filter_tbl_t *fsftp,
	__in		unsigned int index)
{
	EFSYS_ASSERT3P(fsftp->fsft_bitmap, !=, NULL);
	return ((fsftp->fsft_bitmap[index / 32] & (1 << (index % 32))) != 0);
}

static			void
falconsiena_filter_set_used(
	__in		falconsiena_filter_tbl_t *fsftp,
	__in		unsigned int index)
{
	EFSYS_ASSERT3P(fsftp->fsft_bitmap, !=, NULL);
	fsftp->fsft_bitmap[index / 32] |= (1 << (index % 32));
	++fsftp->fsft_used;
}

static			void
falconsiena_filter_clear_used(
	__in		falconsiena_filter_tbl_t *fsftp,
	__in		unsigned int index)
{
	EFSYS_ASSERT3P(fsftp->fsft_bitmap, !=, NULL);
	fsftp->fsft_bitmap[index / 32] &= ~(1 << (index % 32));

	--fsftp->fsft_used;
	EFSYS_ASSERT3U(fsftp->fsft_used, >=, 0);
}


static			falconsiena_filter_tbl_id_t
falconsiena_filter_tbl_id(
	__in		falconsiena_filter_type_t type)
{
	falconsiena_filter_tbl_id_t tbl_id;

	switch (type) {
	case EFX_FS_FILTER_RX_TCP_FULL:
	case EFX_FS_FILTER_RX_TCP_WILD:
	case EFX_FS_FILTER_RX_UDP_FULL:
	case EFX_FS_FILTER_RX_UDP_WILD:
		tbl_id = EFX_FS_FILTER_TBL_RX_IP;
		break;

#if EFSYS_OPT_SIENA
	case EFX_FS_FILTER_RX_MAC_FULL:
	case EFX_FS_FILTER_RX_MAC_WILD:
		tbl_id = EFX_FS_FILTER_TBL_RX_MAC;
		break;

	case EFX_FS_FILTER_TX_TCP_FULL:
	case EFX_FS_FILTER_TX_TCP_WILD:
	case EFX_FS_FILTER_TX_UDP_FULL:
	case EFX_FS_FILTER_TX_UDP_WILD:
		tbl_id = EFX_FS_FILTER_TBL_TX_IP;
		break;

	case EFX_FS_FILTER_TX_MAC_FULL:
	case EFX_FS_FILTER_TX_MAC_WILD:
		tbl_id = EFX_FS_FILTER_TBL_TX_MAC;
		break;
#endif	/* EFSYS_OPT_SIENA */

	default:
		EFSYS_ASSERT(B_FALSE);
		tbl_id = EFX_FS_FILTER_NTBLS;
		break;
	}
	return (tbl_id);
}

static			void
falconsiena_filter_reset_search_depth(
	__inout		falconsiena_filter_t *fsfp,
	__in		falconsiena_filter_tbl_id_t tbl_id)
{
	switch (tbl_id) {
	case EFX_FS_FILTER_TBL_RX_IP:
		fsfp->fsf_depth[EFX_FS_FILTER_RX_TCP_FULL] = 0;
		fsfp->fsf_depth[EFX_FS_FILTER_RX_TCP_WILD] = 0;
		fsfp->fsf_depth[EFX_FS_FILTER_RX_UDP_FULL] = 0;
		fsfp->fsf_depth[EFX_FS_FILTER_RX_UDP_WILD] = 0;
		break;

#if EFSYS_OPT_SIENA
	case EFX_FS_FILTER_TBL_RX_MAC:
		fsfp->fsf_depth[EFX_FS_FILTER_RX_MAC_FULL] = 0;
		fsfp->fsf_depth[EFX_FS_FILTER_RX_MAC_WILD] = 0;
		break;

	case EFX_FS_FILTER_TBL_TX_IP:
		fsfp->fsf_depth[EFX_FS_FILTER_TX_TCP_FULL] = 0;
		fsfp->fsf_depth[EFX_FS_FILTER_TX_TCP_WILD] = 0;
		fsfp->fsf_depth[EFX_FS_FILTER_TX_UDP_FULL] = 0;
		fsfp->fsf_depth[EFX_FS_FILTER_TX_UDP_WILD] = 0;
		break;

	case EFX_FS_FILTER_TBL_TX_MAC:
		fsfp->fsf_depth[EFX_FS_FILTER_TX_MAC_FULL] = 0;
		fsfp->fsf_depth[EFX_FS_FILTER_TX_MAC_WILD] = 0;
		break;
#endif	/* EFSYS_OPT_SIENA */

	default:
		EFSYS_ASSERT(B_FALSE);
		break;
	}
}

static			void
falconsiena_filter_push_rx_limits(
	__in		efx_nic_t *enp)
{
	falconsiena_filter_t *fsfp = enp->en_filter.ef_falconsiena_filter;
	efx_oword_t oword;

	EFX_BAR_READO(enp, FR_AZ_RX_FILTER_CTL_REG, &oword);

	EFX_SET_OWORD_FIELD(oword, FRF_AZ_TCP_FULL_SRCH_LIMIT,
	    fsfp->fsf_depth[EFX_FS_FILTER_RX_TCP_FULL] +
	    FILTER_CTL_SRCH_FUDGE_FULL);
	EFX_SET_OWORD_FIELD(oword, FRF_AZ_TCP_WILD_SRCH_LIMIT,
	    fsfp->fsf_depth[EFX_FS_FILTER_RX_TCP_WILD] +
	    FILTER_CTL_SRCH_FUDGE_WILD);
	EFX_SET_OWORD_FIELD(oword, FRF_AZ_UDP_FULL_SRCH_LIMIT,
	    fsfp->fsf_depth[EFX_FS_FILTER_RX_UDP_FULL] +
	    FILTER_CTL_SRCH_FUDGE_FULL);
	EFX_SET_OWORD_FIELD(oword, FRF_AZ_UDP_WILD_SRCH_LIMIT,
	    fsfp->fsf_depth[EFX_FS_FILTER_RX_UDP_WILD] +
	    FILTER_CTL_SRCH_FUDGE_WILD);

#if EFSYS_OPT_SIENA
	if (fsfp->fsf_tbl[EFX_FS_FILTER_TBL_RX_MAC].fsft_size) {
		EFX_SET_OWORD_FIELD(oword,
		    FRF_CZ_ETHERNET_FULL_SEARCH_LIMIT,
		    fsfp->fsf_depth[EFX_FS_FILTER_RX_MAC_FULL] +
		    FILTER_CTL_SRCH_FUDGE_FULL);
		EFX_SET_OWORD_FIELD(oword,
		    FRF_CZ_ETHERNET_WILDCARD_SEARCH_LIMIT,
		    fsfp->fsf_depth[EFX_FS_FILTER_RX_MAC_WILD] +
		    FILTER_CTL_SRCH_FUDGE_WILD);
	}
#endif /* EFSYS_OPT_SIENA */

	EFX_BAR_WRITEO(enp, FR_AZ_RX_FILTER_CTL_REG, &oword);
}

static			void
falconsiena_filter_push_tx_limits(
	__in		efx_nic_t *enp)
{
	falconsiena_filter_t *fsfp = enp->en_filter.ef_falconsiena_filter;
	efx_oword_t oword;

	EFX_BAR_READO(enp, FR_AZ_TX_CFG_REG, &oword);

	if (fsfp->fsf_tbl[EFX_FS_FILTER_TBL_TX_IP].fsft_size != 0) {
		EFX_SET_OWORD_FIELD(oword,
		    FRF_CZ_TX_TCPIP_FILTER_FULL_SEARCH_RANGE,
		    fsfp->fsf_depth[EFX_FS_FILTER_TX_TCP_FULL] +
		    FILTER_CTL_SRCH_FUDGE_FULL);
		EFX_SET_OWORD_FIELD(oword,
		    FRF_CZ_TX_TCPIP_FILTER_WILD_SEARCH_RANGE,
		    fsfp->fsf_depth[EFX_FS_FILTER_TX_TCP_WILD] +
		    FILTER_CTL_SRCH_FUDGE_WILD);
		EFX_SET_OWORD_FIELD(oword,
		    FRF_CZ_TX_UDPIP_FILTER_FULL_SEARCH_RANGE,
		    fsfp->fsf_depth[EFX_FS_FILTER_TX_UDP_FULL] +
		    FILTER_CTL_SRCH_FUDGE_FULL);
		EFX_SET_OWORD_FIELD(oword,
		    FRF_CZ_TX_UDPIP_FILTER_WILD_SEARCH_RANGE,
		    fsfp->fsf_depth[EFX_FS_FILTER_TX_UDP_WILD] +
		    FILTER_CTL_SRCH_FUDGE_WILD);
	}

	if (fsfp->fsf_tbl[EFX_FS_FILTER_TBL_TX_MAC].fsft_size != 0) {
		EFX_SET_OWORD_FIELD(
			oword, FRF_CZ_TX_ETH_FILTER_FULL_SEARCH_RANGE,
			fsfp->fsf_depth[EFX_FS_FILTER_TX_MAC_FULL] +
			FILTER_CTL_SRCH_FUDGE_FULL);
		EFX_SET_OWORD_FIELD(
			oword, FRF_CZ_TX_ETH_FILTER_WILD_SEARCH_RANGE,
			fsfp->fsf_depth[EFX_FS_FILTER_TX_MAC_WILD] +
			FILTER_CTL_SRCH_FUDGE_WILD);
	}

	EFX_BAR_WRITEO(enp, FR_AZ_TX_CFG_REG, &oword);
}

/* Build a filter entry and return its n-tuple key. */
static	__checkReturn	uint32_t
falconsiena_filter_build(
	__out		efx_oword_t *filter,
	__in		falconsiena_filter_spec_t *spec)
{
	uint32_t dword3;
	uint32_t key;
	uint8_t  type  = spec->fsfs_type;
	uint32_t flags = spec->fsfs_flags;

	switch (falconsiena_filter_tbl_id(type)) {
	case EFX_FS_FILTER_TBL_RX_IP: {
		boolean_t is_udp = (type == EFX_FS_FILTER_RX_UDP_FULL ||
		    type == EFX_FS_FILTER_RX_UDP_WILD);
		EFX_POPULATE_OWORD_7(*filter,
		    FRF_BZ_RSS_EN,
		    (flags & EFX_FILTER_FLAG_RX_RSS) ? 1 : 0,
		    FRF_BZ_SCATTER_EN,
		    (flags & EFX_FILTER_FLAG_RX_SCATTER) ? 1 : 0,
		    FRF_AZ_TCP_UDP, is_udp,
		    FRF_AZ_RXQ_ID, spec->fsfs_dmaq_id,
		    EFX_DWORD_2, spec->fsfs_dword[2],
		    EFX_DWORD_1, spec->fsfs_dword[1],
		    EFX_DWORD_0, spec->fsfs_dword[0]);
		dword3 = is_udp;
		break;
	}

#if EFSYS_OPT_SIENA
	case EFX_FS_FILTER_TBL_RX_MAC: {
		boolean_t is_wild = (type == EFX_FS_FILTER_RX_MAC_WILD);
		EFX_POPULATE_OWORD_7(*filter,
		    FRF_CZ_RMFT_RSS_EN,
		    (flags & EFX_FILTER_FLAG_RX_RSS) ? 1 : 0,
		    FRF_CZ_RMFT_SCATTER_EN,
		    (flags & EFX_FILTER_FLAG_RX_SCATTER) ? 1 : 0,
		    FRF_CZ_RMFT_RXQ_ID, spec->fsfs_dmaq_id,
		    FRF_CZ_RMFT_WILDCARD_MATCH, is_wild,
		    FRF_CZ_RMFT_DEST_MAC_DW1, spec->fsfs_dword[2],
		    FRF_CZ_RMFT_DEST_MAC_DW0, spec->fsfs_dword[1],
		    FRF_CZ_RMFT_VLAN_ID, spec->fsfs_dword[0]);
		dword3 = is_wild;
		break;
	}
#endif /* EFSYS_OPT_SIENA */

	case EFX_FS_FILTER_TBL_TX_IP: {
		boolean_t is_udp = (type == EFX_FS_FILTER_TX_UDP_FULL ||
		    type == EFX_FS_FILTER_TX_UDP_WILD);
		EFX_POPULATE_OWORD_5(*filter,
		    FRF_CZ_TIFT_TCP_UDP, is_udp,
		    FRF_CZ_TIFT_TXQ_ID, spec->fsfs_dmaq_id,
		    EFX_DWORD_2, spec->fsfs_dword[2],
		    EFX_DWORD_1, spec->fsfs_dword[1],
		    EFX_DWORD_0, spec->fsfs_dword[0]);
		dword3 = is_udp | spec->fsfs_dmaq_id << 1;
		break;
	}

#if EFSYS_OPT_SIENA
	case EFX_FS_FILTER_TBL_TX_MAC: {
		boolean_t is_wild = (type == EFX_FS_FILTER_TX_MAC_WILD);
		EFX_POPULATE_OWORD_5(*filter,
		    FRF_CZ_TMFT_TXQ_ID, spec->fsfs_dmaq_id,
		    FRF_CZ_TMFT_WILDCARD_MATCH, is_wild,
		    FRF_CZ_TMFT_SRC_MAC_DW1, spec->fsfs_dword[2],
		    FRF_CZ_TMFT_SRC_MAC_DW0, spec->fsfs_dword[1],
		    FRF_CZ_TMFT_VLAN_ID, spec->fsfs_dword[0]);
		dword3 = is_wild | spec->fsfs_dmaq_id << 1;
		break;
	}
#endif /* EFSYS_OPT_SIENA */

	default:
		EFSYS_ASSERT(B_FALSE);
		return (0);
	}

	key =
	    spec->fsfs_dword[0] ^
	    spec->fsfs_dword[1] ^
	    spec->fsfs_dword[2] ^
	    dword3;

	return (key);
}

static	__checkReturn		efx_rc_t
falconsiena_filter_push_entry(
	__inout			efx_nic_t *enp,
	__in			falconsiena_filter_type_t type,
	__in			int index,
	__in			efx_oword_t *eop)
{
	efx_rc_t rc;

	switch (type) {
	case EFX_FS_FILTER_RX_TCP_FULL:
	case EFX_FS_FILTER_RX_TCP_WILD:
	case EFX_FS_FILTER_RX_UDP_FULL:
	case EFX_FS_FILTER_RX_UDP_WILD:
		EFX_BAR_TBL_WRITEO(enp, FR_AZ_RX_FILTER_TBL0, index,
		    eop, B_TRUE);
		break;

#if EFSYS_OPT_SIENA
	case EFX_FS_FILTER_RX_MAC_FULL:
	case EFX_FS_FILTER_RX_MAC_WILD:
		EFX_BAR_TBL_WRITEO(enp, FR_CZ_RX_MAC_FILTER_TBL0, index,
		    eop, B_TRUE);
		break;

	case EFX_FS_FILTER_TX_TCP_FULL:
	case EFX_FS_FILTER_TX_TCP_WILD:
	case EFX_FS_FILTER_TX_UDP_FULL:
	case EFX_FS_FILTER_TX_UDP_WILD:
		EFX_BAR_TBL_WRITEO(enp, FR_CZ_TX_FILTER_TBL0, index,
		    eop, B_TRUE);
		break;

	case EFX_FS_FILTER_TX_MAC_FULL:
	case EFX_FS_FILTER_TX_MAC_WILD:
		EFX_BAR_TBL_WRITEO(enp, FR_CZ_TX_MAC_FILTER_TBL0, index,
		    eop, B_TRUE);
		break;
#endif	/* EFSYS_OPT_SIENA */

	default:
		EFSYS_ASSERT(B_FALSE);
		rc = ENOTSUP;
		goto fail1;
	}
	return (0);

fail1:
	return (rc);
}


static	__checkReturn	boolean_t
falconsiena_filter_equal(
	__in		const falconsiena_filter_spec_t *left,
	__in		const falconsiena_filter_spec_t *right)
{
	falconsiena_filter_tbl_id_t tbl_id;

	tbl_id = falconsiena_filter_tbl_id(left->fsfs_type);


	if (left->fsfs_type != right->fsfs_type)
		return (B_FALSE);

	if (memcmp(left->fsfs_dword, right->fsfs_dword,
		sizeof (left->fsfs_dword)))
		return (B_FALSE);

	if ((tbl_id == EFX_FS_FILTER_TBL_TX_IP ||
		tbl_id == EFX_FS_FILTER_TBL_TX_MAC) &&
	    left->fsfs_dmaq_id != right->fsfs_dmaq_id)
		return (B_FALSE);

	return (B_TRUE);
}

static	__checkReturn	efx_rc_t
falconsiena_filter_search(
	__in		falconsiena_filter_tbl_t *fsftp,
	__in		falconsiena_filter_spec_t *spec,
	__in		uint32_t key,
	__in		boolean_t for_insert,
	__out		int *filter_index,
	__out		unsigned int *depth_required)
{
	unsigned hash, incr, filter_idx, depth;

	hash = falconsiena_filter_tbl_hash(key);
	incr = falconsiena_filter_tbl_increment(key);

	filter_idx = hash & (fsftp->fsft_size - 1);
	depth = 1;

	for (;;) {
		/*
		 * Return success if entry is used and matches this spec
		 * or entry is unused and we are trying to insert.
		 */
		if (falconsiena_filter_test_used(fsftp, filter_idx) ?
		    falconsiena_filter_equal(spec,
		    &fsftp->fsft_spec[filter_idx]) :
		    for_insert) {
			*filter_index = filter_idx;
			*depth_required = depth;
			return (0);
		}

		/* Return failure if we reached the maximum search depth */
		if (depth == FILTER_CTL_SRCH_MAX)
			return (for_insert ? EBUSY : ENOENT);

		filter_idx = (filter_idx + incr) & (fsftp->fsft_size - 1);
		++depth;
	}
}

static			void
falconsiena_filter_clear_entry(
	__in		efx_nic_t *enp,
	__in		falconsiena_filter_tbl_t *fsftp,
	__in		int index)
{
	efx_oword_t filter;

	if (falconsiena_filter_test_used(fsftp, index)) {
		falconsiena_filter_clear_used(fsftp, index);

		EFX_ZERO_OWORD(filter);
		falconsiena_filter_push_entry(enp,
		    fsftp->fsft_spec[index].fsfs_type,
		    index, &filter);

		memset(&fsftp->fsft_spec[index],
		    0, sizeof (fsftp->fsft_spec[0]));
	}
}

			void
falconsiena_filter_tbl_clear(
	__in		efx_nic_t *enp,
	__in		falconsiena_filter_tbl_id_t tbl_id)
{
	falconsiena_filter_t *fsfp = enp->en_filter.ef_falconsiena_filter;
	falconsiena_filter_tbl_t *fsftp = &fsfp->fsf_tbl[tbl_id];
	int index;
	int state;

	EFSYS_LOCK(enp->en_eslp, state);

	for (index = 0; index < fsftp->fsft_size; ++index) {
		falconsiena_filter_clear_entry(enp, fsftp, index);
	}

	if (fsftp->fsft_used == 0)
		falconsiena_filter_reset_search_depth(fsfp, tbl_id);

	EFSYS_UNLOCK(enp->en_eslp, state);
}

static	__checkReturn	efx_rc_t
falconsiena_filter_init(
	__in		efx_nic_t *enp)
{
	falconsiena_filter_t *fsfp;
	falconsiena_filter_tbl_t *fsftp;
	int tbl_id;
	efx_rc_t rc;

	EFSYS_KMEM_ALLOC(enp->en_esip, sizeof (falconsiena_filter_t), fsfp);

	if (!fsfp) {
		rc = ENOMEM;
		goto fail1;
	}

	enp->en_filter.ef_falconsiena_filter = fsfp;

	switch (enp->en_family) {
#if EFSYS_OPT_FALCON
	case EFX_FAMILY_FALCON:
		fsftp = &fsfp->fsf_tbl[EFX_FS_FILTER_TBL_RX_IP];
		fsftp->fsft_size = FR_AZ_RX_FILTER_TBL0_ROWS;
		break;
#endif	/* EFSYS_OPT_FALCON */

#if EFSYS_OPT_SIENA
	case EFX_FAMILY_SIENA:
		fsftp = &fsfp->fsf_tbl[EFX_FS_FILTER_TBL_RX_IP];
		fsftp->fsft_size = FR_AZ_RX_FILTER_TBL0_ROWS;

		fsftp = &fsfp->fsf_tbl[EFX_FS_FILTER_TBL_RX_MAC];
		fsftp->fsft_size = FR_CZ_RX_MAC_FILTER_TBL0_ROWS;

		fsftp = &fsfp->fsf_tbl[EFX_FS_FILTER_TBL_TX_IP];
		fsftp->fsft_size = FR_CZ_TX_FILTER_TBL0_ROWS;

		fsftp = &fsfp->fsf_tbl[EFX_FS_FILTER_TBL_TX_MAC];
		fsftp->fsft_size = FR_CZ_TX_MAC_FILTER_TBL0_ROWS;
		break;
#endif	/* EFSYS_OPT_SIENA */

	default:
		rc = ENOTSUP;
		goto fail2;
	}

	for (tbl_id = 0; tbl_id < EFX_FS_FILTER_NTBLS; tbl_id++) {
		unsigned int bitmap_size;

		fsftp = &fsfp->fsf_tbl[tbl_id];
		if (fsftp->fsft_size == 0)
			continue;

		EFX_STATIC_ASSERT(sizeof (fsftp->fsft_bitmap[0]) ==
		    sizeof (uint32_t));
		bitmap_size =
		    (fsftp->fsft_size + (sizeof (uint32_t) * 8) - 1) / 8;

		EFSYS_KMEM_ALLOC(enp->en_esip, bitmap_size, fsftp->fsft_bitmap);
		if (!fsftp->fsft_bitmap) {
			rc = ENOMEM;
			goto fail3;
		}

		EFSYS_KMEM_ALLOC(enp->en_esip,
		    fsftp->fsft_size * sizeof (*fsftp->fsft_spec),
		    fsftp->fsft_spec);
		if (!fsftp->fsft_spec) {
			rc = ENOMEM;
			goto fail4;
		}
		memset(fsftp->fsft_spec, 0,
		    fsftp->fsft_size * sizeof (*fsftp->fsft_spec));
	}

	return (0);

fail4:
	EFSYS_PROBE(fail4);

fail3:
	EFSYS_PROBE(fail3);

fail2:
	EFSYS_PROBE(fail2);
	falconsiena_filter_fini(enp);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

static			void
falconsiena_filter_fini(
	__in		efx_nic_t *enp)
{
	falconsiena_filter_t *fsfp = enp->en_filter.ef_falconsiena_filter;
	falconsiena_filter_tbl_id_t tbl_id;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PROBE);

	if (fsfp == NULL)
		return;

	for (tbl_id = 0; tbl_id < EFX_FS_FILTER_NTBLS; tbl_id++) {
		falconsiena_filter_tbl_t *fsftp = &fsfp->fsf_tbl[tbl_id];
		unsigned int bitmap_size;

		EFX_STATIC_ASSERT(sizeof (fsftp->fsft_bitmap[0]) ==
		    sizeof (uint32_t));
		bitmap_size =
		    (fsftp->fsft_size + (sizeof (uint32_t) * 8) - 1) / 8;

		if (fsftp->fsft_bitmap != NULL) {
			EFSYS_KMEM_FREE(enp->en_esip, bitmap_size,
			    fsftp->fsft_bitmap);
			fsftp->fsft_bitmap = NULL;
		}

		if (fsftp->fsft_spec != NULL) {
			EFSYS_KMEM_FREE(enp->en_esip, fsftp->fsft_size *
			    sizeof (*fsftp->fsft_spec), fsftp->fsft_spec);
			fsftp->fsft_spec = NULL;
		}
	}

	EFSYS_KMEM_FREE(enp->en_esip, sizeof (falconsiena_filter_t),
	    enp->en_filter.ef_falconsiena_filter);
}

/* Restore filter state after a reset */
static	__checkReturn	efx_rc_t
falconsiena_filter_restore(
	__in		efx_nic_t *enp)
{
	falconsiena_filter_t *fsfp = enp->en_filter.ef_falconsiena_filter;
	falconsiena_filter_tbl_id_t tbl_id;
	falconsiena_filter_tbl_t *fsftp;
	falconsiena_filter_spec_t *spec;
	efx_oword_t filter;
	int filter_idx;
	int state;
	efx_rc_t rc;

	EFSYS_LOCK(enp->en_eslp, state);

	for (tbl_id = 0; tbl_id < EFX_FS_FILTER_NTBLS; tbl_id++) {
		fsftp = &fsfp->fsf_tbl[tbl_id];
		for (filter_idx = 0;
			filter_idx < fsftp->fsft_size;
			filter_idx++) {
			if (!falconsiena_filter_test_used(fsftp, filter_idx))
				continue;

			spec = &fsftp->fsft_spec[filter_idx];
			if ((rc = falconsiena_filter_build(&filter, spec)) != 0)
				goto fail1;
			if ((rc = falconsiena_filter_push_entry(enp,
				    spec->fsfs_type, filter_idx, &filter)) != 0)
				goto fail2;
		}
	}

	falconsiena_filter_push_rx_limits(enp);
	falconsiena_filter_push_tx_limits(enp);

	EFSYS_UNLOCK(enp->en_eslp, state);

	return (0);

fail2:
	EFSYS_PROBE(fail2);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	EFSYS_UNLOCK(enp->en_eslp, state);

	return (rc);
}

static	 __checkReturn	efx_rc_t
falconsiena_filter_add(
	__in		efx_nic_t *enp,
	__inout		efx_filter_spec_t *spec,
	__in		boolean_t may_replace)
{
	efx_rc_t rc;
	falconsiena_filter_spec_t fs_spec;
	falconsiena_filter_t *fsfp = enp->en_filter.ef_falconsiena_filter;
	falconsiena_filter_tbl_id_t tbl_id;
	falconsiena_filter_tbl_t *fsftp;
	falconsiena_filter_spec_t *saved_fs_spec;
	efx_oword_t filter;
	int filter_idx;
	unsigned int depth;
	int state;
	uint32_t key;


	EFSYS_ASSERT3P(spec, !=, NULL);

	if ((rc = falconsiena_filter_spec_from_gen_spec(&fs_spec, spec)) != 0)
		goto fail1;

	tbl_id = falconsiena_filter_tbl_id(fs_spec.fsfs_type);
	fsftp = &fsfp->fsf_tbl[tbl_id];

	if (fsftp->fsft_size == 0) {
		rc = EINVAL;
		goto fail2;
	}

	key = falconsiena_filter_build(&filter, &fs_spec);

	EFSYS_LOCK(enp->en_eslp, state);

	rc = falconsiena_filter_search(fsftp, &fs_spec, key, B_TRUE,
	    &filter_idx, &depth);
	if (rc != 0)
		goto fail3;

	EFSYS_ASSERT3U(filter_idx, <, fsftp->fsft_size);
	saved_fs_spec = &fsftp->fsft_spec[filter_idx];

	if (falconsiena_filter_test_used(fsftp, filter_idx)) {
		if (may_replace == B_FALSE) {
			rc = EEXIST;
			goto fail4;
		}
	}
	falconsiena_filter_set_used(fsftp, filter_idx);
	*saved_fs_spec = fs_spec;

	if (fsfp->fsf_depth[fs_spec.fsfs_type] < depth) {
		fsfp->fsf_depth[fs_spec.fsfs_type] = depth;
		if (tbl_id == EFX_FS_FILTER_TBL_TX_IP ||
		    tbl_id == EFX_FS_FILTER_TBL_TX_MAC)
			falconsiena_filter_push_tx_limits(enp);
		else
			falconsiena_filter_push_rx_limits(enp);
	}

	falconsiena_filter_push_entry(enp, fs_spec.fsfs_type,
	    filter_idx, &filter);

	EFSYS_UNLOCK(enp->en_eslp, state);
	return (0);

fail4:
	EFSYS_PROBE(fail4);

fail3:
	EFSYS_UNLOCK(enp->en_eslp, state);
	EFSYS_PROBE(fail3);

fail2:
	EFSYS_PROBE(fail2);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

static	 __checkReturn	efx_rc_t
falconsiena_filter_delete(
	__in		efx_nic_t *enp,
	__inout		efx_filter_spec_t *spec)
{
	efx_rc_t rc;
	falconsiena_filter_spec_t fs_spec;
	falconsiena_filter_t *fsfp = enp->en_filter.ef_falconsiena_filter;
	falconsiena_filter_tbl_id_t tbl_id;
	falconsiena_filter_tbl_t *fsftp;
	falconsiena_filter_spec_t *saved_spec;
	efx_oword_t filter;
	int filter_idx;
	unsigned int depth;
	int state;
	uint32_t key;

	EFSYS_ASSERT3P(spec, !=, NULL);

	if ((rc = falconsiena_filter_spec_from_gen_spec(&fs_spec, spec)) != 0)
		goto fail1;

	tbl_id = falconsiena_filter_tbl_id(fs_spec.fsfs_type);
	fsftp = &fsfp->fsf_tbl[tbl_id];

	key = falconsiena_filter_build(&filter, &fs_spec);

	EFSYS_LOCK(enp->en_eslp, state);

	rc = falconsiena_filter_search(fsftp, &fs_spec, key, B_FALSE,
	    &filter_idx, &depth);
	if (rc != 0)
		goto fail2;

	saved_spec = &fsftp->fsft_spec[filter_idx];

	falconsiena_filter_clear_entry(enp, fsftp, filter_idx);
	if (fsftp->fsft_used == 0)
		falconsiena_filter_reset_search_depth(fsfp, tbl_id);

	EFSYS_UNLOCK(enp->en_eslp, state);
	return (0);

fail2:
	EFSYS_UNLOCK(enp->en_eslp, state);
	EFSYS_PROBE(fail2);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

#define	MAX_SUPPORTED 4

static	__checkReturn	efx_rc_t
falconsiena_filter_supported_filters(
	__in		efx_nic_t *enp,
	__out		uint32_t *list,
	__out		size_t *length)
{
	int index = 0;
	uint32_t rx_matches[MAX_SUPPORTED];
	efx_rc_t rc;

	if (list == NULL) {
		rc = EINVAL;
		goto fail1;
	}

	rx_matches[index++] =
	    EFX_FILTER_MATCH_ETHER_TYPE | EFX_FILTER_MATCH_IP_PROTO |
	    EFX_FILTER_MATCH_LOC_HOST | EFX_FILTER_MATCH_LOC_PORT |
	    EFX_FILTER_MATCH_REM_HOST | EFX_FILTER_MATCH_REM_PORT;

	rx_matches[index++] =
	    EFX_FILTER_MATCH_ETHER_TYPE | EFX_FILTER_MATCH_IP_PROTO |
	    EFX_FILTER_MATCH_LOC_HOST | EFX_FILTER_MATCH_LOC_PORT;

	if (enp->en_features & EFX_FEATURE_MAC_HEADER_FILTERS) {
		rx_matches[index++] =
		    EFX_FILTER_MATCH_OUTER_VID | EFX_FILTER_MATCH_LOC_MAC;

		rx_matches[index++] = EFX_FILTER_MATCH_LOC_MAC;
	}

	EFSYS_ASSERT3U(index, <=, MAX_SUPPORTED);

	*length = index;
	memcpy(list, rx_matches, *length);

	return (0);

fail1:

	return (rc);
}

#undef MAX_SUPPORTED

#endif /* EFSYS_OPT_FALCON || EFSYS_OPT_SIENA */

#endif /* EFSYS_OPT_FILTER */
