/*-
 * Copyright (c) 2006-2008 University of Zagreb
 * Copyright (c) 2006-2008 FreeBSD Foundation
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

#ifndef	_SYS_VIMAGE_H_
#define	_SYS_VIMAGE_H_

#define	V_hostname hostname
#define	G_hostname hostname
#define	V_domainname domainname
#define	V_acq_seq acq_seq
#define	V_acqtree acqtree
#define	V_addrsel_policytab addrsel_policytab
#define	V_ah_cleartos ah_cleartos
#define	V_ah_enable ah_enable
#define	V_ahstat ahstat
#define	V_arp_maxtries arp_maxtries
#define	V_arp_proxyall arp_proxyall
#define	V_arpt_keep arpt_keep
#define	V_autoinc_step autoinc_step
#define	V_blackhole blackhole
#define	V_crypto_support crypto_support
#define	V_curr_dyn_buckets curr_dyn_buckets
#define	V_dad_ignore_ns dad_ignore_ns
#define	V_dad_init dad_init
#define	V_dad_maxtry dad_maxtry
#define	V_dadq dadq
#define	V_defaultaddrpolicy defaultaddrpolicy
#define	V_divcb divcb
#define	V_divcbinfo divcbinfo
#define	V_drop_synfin drop_synfin
#define	V_dyn_ack_lifetime dyn_ack_lifetime
#define	V_dyn_buckets dyn_buckets
#define	V_dyn_count dyn_count
#define	V_dyn_fin_lifetime dyn_fin_lifetime
#define	V_dyn_keepalive dyn_keepalive
#define	V_dyn_keepalive_interval dyn_keepalive_interval
#define	V_dyn_keepalive_period dyn_keepalive_period
#define	V_dyn_max dyn_max
#define	V_dyn_rst_lifetime dyn_rst_lifetime
#define	V_dyn_short_lifetime dyn_short_lifetime
#define	V_dyn_syn_lifetime dyn_syn_lifetime
#define	V_dyn_udp_lifetime dyn_udp_lifetime
#define	V_esp_enable esp_enable
#define	V_esp_max_ivlen esp_max_ivlen
#define	V_espstat espstat
#define	V_ether_ipfw ether_ipfw
#define	V_frag6_nfragpackets frag6_nfragpackets
#define	V_frag6_nfrags frag6_nfrags
#define	V_fw6_enable fw6_enable
#define	V_fw_debug fw_debug
#define	V_fw_deny_unknown_exthdrs fw_deny_unknown_exthdrs
#define	V_fw_enable fw_enable
#define	V_fw_one_pass fw_one_pass
#define	V_fw_verbose fw_verbose
#define	V_gif_softc_list gif_softc_list
#define	V_icmp6_nodeinfo icmp6_nodeinfo
#define	V_icmp6_rediraccept icmp6_rediraccept
#define	V_icmp6_redirtimeout icmp6_redirtimeout
#define	V_icmp6errpps_count icmp6errpps_count
#define	V_icmp6errppslim icmp6errppslim
#define	V_icmp6errppslim_last icmp6errppslim_last
#define	V_icmp6stat icmp6stat
#define	V_icmp_may_rst icmp_may_rst
#define	V_icmpstat icmpstat
#define	V_if_index if_index
#define	V_if_indexlim if_indexlim
#define	V_ifaddr_event_tag ifaddr_event_tag
#define	V_ifg_head ifg_head
#define	V_ifindex_table ifindex_table
#define	V_ifklist ifklist
#define	V_ifnet ifnet
#define	V_igmpstat igmpstat
#define	V_in6_ifaddr in6_ifaddr
#define	V_in6_maxmtu in6_maxmtu
#define	V_in6_tmpaddrtimer_ch in6_tmpaddrtimer_ch
#define	V_in_ifaddrhashtbl in_ifaddrhashtbl
#define	V_in_ifaddrhead in_ifaddrhead
#define	V_in_ifaddrhmask in_ifaddrhmask
#define	V_in_multihead in_multihead
#define	V_ip4_ah_net_deflev ip4_ah_net_deflev
#define	V_ip4_ah_offsetmask ip4_ah_offsetmask
#define	V_ip4_ah_trans_deflev ip4_ah_trans_deflev
#define	V_ip4_def_policy ip4_def_policy
#define	V_ip4_esp_net_deflev ip4_esp_net_deflev
#define	V_ip4_esp_randpad ip4_esp_randpad
#define	V_ip4_esp_trans_deflev ip4_esp_trans_deflev
#define	V_ip4_ipsec_dfbit ip4_ipsec_dfbit
#define	V_ip4_ipsec_ecn ip4_ipsec_ecn
#define	V_ip6_accept_rtadv ip6_accept_rtadv
#define	V_ip6_ah_net_deflev ip6_ah_net_deflev
#define	V_ip6_ah_trans_deflev ip6_ah_trans_deflev
#define	V_ip6_auto_flowlabel ip6_auto_flowlabel
#define	V_ip6_auto_linklocal ip6_auto_linklocal
#define	V_ip6_dad_count ip6_dad_count
#define	V_ip6_defhlim ip6_defhlim
#define	V_ip6_defmcasthlim ip6_defmcasthlim
#define	V_ip6_desync_factor ip6_desync_factor
#define	V_ip6_esp_net_deflev ip6_esp_net_deflev
#define	V_ip6_esp_trans_deflev ip6_esp_trans_deflev
#define	V_ip6_forward_rt ip6_forward_rt
#define	V_ip6_forward_srcrt ip6_forward_srcrt
#define	V_ip6_forwarding ip6_forwarding
#define	V_ip6_gif_hlim ip6_gif_hlim
#define	V_ip6_hdrnestlimit ip6_hdrnestlimit
#define	V_ip6_ipsec_ecn ip6_ipsec_ecn
#define	V_ip6_keepfaith ip6_keepfaith
#define	V_ip6_log_interval ip6_log_interval
#define	V_ip6_log_time ip6_log_time
#define	V_ip6_maxfragpackets ip6_maxfragpackets
#define	V_ip6_maxfrags ip6_maxfrags
#define	V_ip6_mcast_pmtu ip6_mcast_pmtu
#define	V_ip6_mrouter_ver ip6_mrouter_ver
#define	V_ip6_opts ip6_opts
#define	V_ip6_ours_check_algorithm ip6_ours_check_algorithm
#define	V_ip6_prefer_tempaddr ip6_prefer_tempaddr
#define	V_ip6_rr_prune ip6_rr_prune
#define	V_ip6_sendredirects ip6_sendredirects
#define	V_ip6_sourcecheck ip6_sourcecheck
#define	V_ip6_sourcecheck_interval ip6_sourcecheck_interval
#define	V_ip6_temp_preferred_lifetime ip6_temp_preferred_lifetime
#define	V_ip6_temp_regen_advance ip6_temp_regen_advance
#define	V_ip6_temp_valid_lifetime ip6_temp_valid_lifetime
#define	V_ip6_use_defzone ip6_use_defzone
#define	V_ip6_use_deprecated ip6_use_deprecated
#define	V_ip6_use_tempaddr ip6_use_tempaddr
#define	V_ip6_v6only ip6_v6only
#define	V_ip6q ip6q
#define	V_ip6qmaxlen ip6qmaxlen
#define	V_ip6stat ip6stat
#define	V_ip6stealth ip6stealth
#define	V_ip_checkinterface ip_checkinterface
#define	V_ip_defttl ip_defttl
#define	V_ip_do_randomid ip_do_randomid
#define	V_ip_gif_ttl ip_gif_ttl
#define	V_ip_keepfaith ip_keepfaith
#define	V_ip_mrouter ip_mrouter
#define	V_ip_rsvp_on ip_rsvp_on
#define	V_ip_rsvpd ip_rsvpd
#define	V_ip_sendsourcequench ip_sendsourcequench
#define	V_ipcomp_enable ipcomp_enable
#define	V_ipcompstat ipcompstat
#define	V_ipfastforward_active ipfastforward_active
#define	V_ipforwarding ipforwarding
#define	V_ipfw_dyn_v ipfw_dyn_v
#define	V_ipfw_timeout ipfw_timeout
#define	V_ipip_allow ipip_allow
#define	V_ipipstat ipipstat
#define	V_ipport_firstauto ipport_firstauto
#define	V_ipport_hifirstauto ipport_hifirstauto
#define	V_ipport_hilastauto ipport_hilastauto
#define	V_ipport_lastauto ipport_lastauto
#define	V_ipport_lowfirstauto ipport_lowfirstauto
#define	V_ipport_lowlastauto ipport_lowlastauto
#define	V_ipport_randomcps ipport_randomcps
#define	V_ipport_randomized ipport_randomized
#define	V_ipport_randomtime ipport_randomtime
#define	V_ipport_reservedhigh ipport_reservedhigh
#define	V_ipport_reservedlow ipport_reservedlow
#define	V_ipport_stoprandom ipport_stoprandom
#define	V_ipport_tcpallocs ipport_tcpallocs
#define	V_ipport_tcplastcount ipport_tcplastcount
#define	V_ipq ipq
#define	V_ipq_zone ipq_zone
#define	V_ipsec4stat ipsec4stat
#define	V_ipsec6stat ipsec6stat
#define	V_ipsec_ah_keymin ipsec_ah_keymin
#define	V_ipsec_debug ipsec_debug
#define	V_ipsec_esp_auth ipsec_esp_auth
#define	V_ipsec_esp_keymin ipsec_esp_keymin
#define	V_ipsec_integrity ipsec_integrity
#define	V_ipsec_replay ipsec_replay
#define	V_ipsendredirects ipsendredirects
#define	V_ipstat ipstat
#define	V_ipstealth ipstealth
#define	V_isn_ctx isn_ctx
#define	V_isn_last_reseed isn_last_reseed
#define	V_isn_offset isn_offset
#define	V_isn_offset_old isn_offset_old
#define	V_isn_secret isn_secret
#define	V_key_blockacq_count key_blockacq_count
#define	V_key_blockacq_lifetime key_blockacq_lifetime
#define	V_key_cb key_cb
#define	V_key_debug_level key_debug_level
#define	V_key_int_random key_int_random
#define	V_key_larval_lifetime key_larval_lifetime
#define	V_key_preferred_oldsa key_preferred_oldsa
#define	V_key_spi_maxval key_spi_maxval
#define	V_key_spi_minval key_spi_minval
#define	V_key_spi_trycnt key_spi_trycnt
#define	V_key_src key_src
#define	V_layer3_chain layer3_chain
#define	V_llinfo_arp llinfo_arp
#define	V_llinfo_nd6 llinfo_nd6
#define	V_lo_list lo_list
#define	V_loif loif
#define	V_max_gif_nesting max_gif_nesting
#define	V_maxfragsperpacket maxfragsperpacket
#define	V_maxnipq maxnipq
#define	V_mrt6debug mrt6debug
#define	V_nd6_allocated nd6_allocated
#define	V_nd6_debug nd6_debug
#define	V_nd6_defifindex nd6_defifindex
#define	V_nd6_defifp nd6_defifp
#define	V_nd6_delay nd6_delay
#define	V_nd6_gctimer nd6_gctimer
#define	V_nd6_inuse nd6_inuse
#define	V_nd6_maxndopt nd6_maxndopt
#define	V_nd6_maxnudhint nd6_maxnudhint
#define	V_nd6_maxqueuelen nd6_maxqueuelen
#define	V_nd6_mmaxtries nd6_mmaxtries
#define	V_nd6_prune nd6_prune
#define	V_nd6_recalc_reachtm_interval nd6_recalc_reachtm_interval
#define	V_nd6_slowtimo_ch nd6_slowtimo_ch
#define	V_nd6_timer_ch nd6_timer_ch
#define	V_nd6_umaxtries nd6_umaxtries
#define	V_nd6_useloopback nd6_useloopback
#define	V_nd_defrouter nd_defrouter
#define	V_nd_prefix nd_prefix
#define	V_nextID nextID
#define	V_ng_ID_hash ng_ID_hash
#define	V_ng_eiface_unit ng_eiface_unit
#define	V_ng_iface_unit ng_iface_unit
#define	V_ng_name_hash ng_name_hash
#define	V_nipq nipq
#define	V_nolocaltimewait nolocaltimewait
#define	V_norule_counter norule_counter
#define	V_parallel_tunnels parallel_tunnels
#define	V_path_mtu_discovery path_mtu_discovery
#define	V_pfkeystat pfkeystat
#define	V_pim6 pim6
#define	V_pmtu_expire pmtu_expire
#define	V_pmtu_probe pmtu_probe
#define	V_policy_id policy_id
#define	V_rawcb_list rawcb_list
#define	V_regtree regtree
#define	V_rip6_recvspace rip6_recvspace
#define	V_rip6_sendspace rip6_sendspace
#define	V_rip6stat rip6stat
#define	V_ripcb ripcb
#define	V_ripcbinfo ripcbinfo
#define	V_router_info_head router_info_head
#define	V_rsvp_on rsvp_on
#define	V_rt_tables rt_tables
#define	V_rtq_minreallyold rtq_minreallyold
#define	V_rtq_minreallyold6 rtq_minreallyold6
#define	V_rtq_mtutimer rtq_mtutimer
#define	V_rtq_reallyold rtq_reallyold
#define	V_rtq_reallyold6 rtq_reallyold6
#define	V_rtq_timeout rtq_timeout
#define	V_rtq_timeout6 rtq_timeout6
#define	V_rtq_timer rtq_timer
#define	V_rtq_timer6 rtq_timer6
#define	V_rtq_toomany rtq_toomany
#define	V_rtq_toomany6 rtq_toomany6
#define	V_rtstat rtstat
#define	V_rttrash rttrash
#define	V_sahtree sahtree
#define	V_sameprefixcarponly sameprefixcarponly
#define	V_saorder_state_alive saorder_state_alive
#define	V_saorder_state_any saorder_state_any
#define	V_set_disable set_disable
#define	V_sid_default sid_default
#define	V_spacqtree spacqtree
#define	V_sptree sptree
#define	V_ss_fltsz ss_fltsz
#define	V_ss_fltsz_local ss_fltsz_local
#define	V_static_count static_count
#define	V_subnetsarelocal subnetsarelocal
#define	V_tcb tcb
#define	V_tcbinfo tcbinfo
#define	V_tcp_autorcvbuf_inc tcp_autorcvbuf_inc
#define	V_tcp_autorcvbuf_max tcp_autorcvbuf_max
#define	V_tcp_autosndbuf_inc tcp_autosndbuf_inc
#define	V_tcp_autosndbuf_max tcp_autosndbuf_max
#define	V_tcp_delack_enabled tcp_delack_enabled
#define	V_tcp_do_autorcvbuf tcp_do_autorcvbuf
#define	V_tcp_do_autosndbuf tcp_do_autosndbuf
#define	V_tcp_do_ecn tcp_do_ecn
#define	V_tcp_do_newreno tcp_do_newreno
#define	V_tcp_do_rfc1323 tcp_do_rfc1323
#define	V_tcp_do_rfc3042 tcp_do_rfc3042
#define	V_tcp_do_rfc3390 tcp_do_rfc3390
#define	V_tcp_do_sack tcp_do_sack
#define	V_tcp_do_tso tcp_do_tso
#define	V_tcp_hc_callout tcp_hc_callout
#define	V_tcp_ecn_maxretries tcp_ecn_maxretries
#define	V_tcp_hostcache tcp_hostcache
#define	V_tcp_inflight_enable tcp_inflight_enable
#define	V_tcp_inflight_max tcp_inflight_max
#define	V_tcp_inflight_min tcp_inflight_min
#define	V_tcp_inflight_rttthresh tcp_inflight_rttthresh
#define	V_tcp_inflight_stab tcp_inflight_stab
#define	V_tcp_insecure_rst tcp_insecure_rst
#define	V_tcp_isn_reseed_interval tcp_isn_reseed_interval
#define	V_tcp_minmss tcp_minmss
#define	V_tcp_mssdflt tcp_mssdflt
#define	V_tcp_reass_maxqlen tcp_reass_maxqlen
#define	V_tcp_reass_maxseg tcp_reass_maxseg
#define	V_tcp_reass_overflows tcp_reass_overflows
#define	V_tcp_reass_qsize tcp_reass_qsize
#define	V_tcp_sack_globalholes tcp_sack_globalholes
#define	V_tcp_sack_globalmaxholes tcp_sack_globalmaxholes
#define	V_tcp_sack_maxholes tcp_sack_maxholes
#define	V_tcp_sc_rst_sock_fail tcp_sc_rst_sock_fail
#define	V_tcp_syncache tcp_syncache
#define	V_tcp_v6mssdflt tcp_v6mssdflt
#define	V_tcpstat tcpstat
#define	V_twq_2msl twq_2msl
#define	V_udb udb
#define	V_udbinfo udbinfo
#define	V_udp_blackhole udp_blackhole
#define	V_udp6_recvspace udp6_recvspace
#define	V_udp6_sendspace udp6_sendspace
#define	V_udpstat udpstat
#define	V_useloopback useloopback
#define	V_verbose_limit verbose_limit

#endif /* !_SYS_VIMAGE_H_ */
