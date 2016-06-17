/*
 *  linux/net/netsyms.c
 *
 *  Symbol table for the linux networking subsystem. Moved here to
 *  make life simpler in ksyms.c.
 */

#include <linux/config.h>
#include <linux/module.h>

#include <linux/types.h>
#include <linux/net.h>
#include <linux/in.h>
#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <linux/fddidevice.h>
#include <linux/trdevice.h>
#include <linux/fcdevice.h>
#include <linux/ioport.h>
#include <linux/tty.h>
#include <linux/ethtool.h>
#include <net/neighbour.h>
#include <net/snmp.h>
#include <net/dst.h>
#include <net/checksum.h>
#include <linux/etherdevice.h>
#include <net/route.h>
#ifdef CONFIG_HIPPI
#include <linux/hippidevice.h>
#endif
#include <net/pkt_sched.h>
#include <net/scm.h>
#include <linux/if_bridge.h>
#include <linux/if_vlan.h>
#include <linux/random.h>
#ifdef CONFIG_NET_DIVERT
#include <linux/divert.h>
#endif /* CONFIG_NET_DIVERT */

#ifdef CONFIG_NET
extern __u32 sysctl_wmem_max;
extern __u32 sysctl_rmem_max;
extern int sysctl_optmem_max;
#endif

#ifdef CONFIG_INET
#include <linux/ip.h>
#include <net/protocol.h>
#include <net/arp.h>
#if defined(CONFIG_ATM_CLIP) || defined(CONFIG_ATM_CLIP_MODULE)
#include <net/atmclip.h>
#endif
#include <net/ip.h>
#include <net/udp.h>
#include <net/tcp.h>
#include <net/icmp.h>
#include <net/inet_common.h>
#include <linux/inet.h>
#include <linux/mroute.h>
#include <linux/igmp.h>

extern struct net_proto_family inet_family_ops;

#if defined(CONFIG_IPV6) || defined (CONFIG_IPV6_MODULE) || defined (CONFIG_KHTTPD) || defined (CONFIG_KHTTPD_MODULE) || defined (CONFIG_IP_SCTP_MODULE)
#include <linux/in6.h>
#include <linux/icmpv6.h>
#include <net/ipv6.h>
#include <net/ndisc.h>
#include <net/transp_v6.h>
#include <net/addrconf.h>

extern int sysctl_local_port_range[2];
extern int tcp_port_rover;
extern int udp_port_rover;
#endif

#endif

extern int netdev_finish_unregister(struct net_device *dev);

#include <linux/rtnetlink.h>

#ifdef CONFIG_IPX_MODULE
extern struct datalink_proto   *make_EII_client(void);
extern struct datalink_proto   *make_8023_client(void);
extern void destroy_EII_client(struct datalink_proto *);
extern void destroy_8023_client(struct datalink_proto *);
#endif

#ifdef CONFIG_ATALK_MODULE
#include <net/sock.h>
#endif

#ifdef CONFIG_SYSCTL
extern int sysctl_max_syn_backlog;
#endif

/* Skbuff symbols. */
EXPORT_SYMBOL(skb_over_panic);
EXPORT_SYMBOL(skb_under_panic);
EXPORT_SYMBOL(skb_pad);

/* Socket layer registration */
EXPORT_SYMBOL(sock_register);
EXPORT_SYMBOL(sock_unregister);

/* Socket locking */
EXPORT_SYMBOL(__lock_sock);
EXPORT_SYMBOL(__release_sock);

/* Socket layer support routines */
EXPORT_SYMBOL(memcpy_fromiovec);
EXPORT_SYMBOL(memcpy_tokerneliovec);
EXPORT_SYMBOL(sock_create);
EXPORT_SYMBOL(sock_alloc);
EXPORT_SYMBOL(sock_release);
EXPORT_SYMBOL(sock_setsockopt);
EXPORT_SYMBOL(sock_getsockopt);
EXPORT_SYMBOL(sock_sendmsg);
EXPORT_SYMBOL(sock_recvmsg);
EXPORT_SYMBOL(sk_alloc);
EXPORT_SYMBOL(sk_free);
EXPORT_SYMBOL(sock_wake_async);
EXPORT_SYMBOL(sock_alloc_send_skb);
EXPORT_SYMBOL(sock_alloc_send_pskb);
EXPORT_SYMBOL(sock_init_data);
EXPORT_SYMBOL(sock_no_release);
EXPORT_SYMBOL(sock_no_bind);
EXPORT_SYMBOL(sock_no_connect);
EXPORT_SYMBOL(sock_no_socketpair);
EXPORT_SYMBOL(sock_no_accept);
EXPORT_SYMBOL(sock_no_getname);
EXPORT_SYMBOL(sock_no_poll);
EXPORT_SYMBOL(sock_no_ioctl);
EXPORT_SYMBOL(sock_no_listen);
EXPORT_SYMBOL(sock_no_shutdown);
EXPORT_SYMBOL(sock_no_getsockopt);
EXPORT_SYMBOL(sock_no_setsockopt);
EXPORT_SYMBOL(sock_no_sendmsg);
EXPORT_SYMBOL(sock_no_recvmsg);
EXPORT_SYMBOL(sock_no_mmap);
EXPORT_SYMBOL(sock_no_sendpage);
EXPORT_SYMBOL(sock_rfree);
EXPORT_SYMBOL(sock_wfree);
EXPORT_SYMBOL(sock_wmalloc);
EXPORT_SYMBOL(sock_rmalloc);
EXPORT_SYMBOL(skb_linearize);
EXPORT_SYMBOL(skb_checksum);
EXPORT_SYMBOL(skb_checksum_help);
EXPORT_SYMBOL(skb_recv_datagram);
EXPORT_SYMBOL(skb_free_datagram);
EXPORT_SYMBOL(skb_copy_datagram);
EXPORT_SYMBOL(skb_copy_datagram_iovec);
EXPORT_SYMBOL(skb_copy_and_csum_datagram_iovec);
EXPORT_SYMBOL(skb_copy_bits);
EXPORT_SYMBOL(skb_copy_and_csum_bits);
EXPORT_SYMBOL(skb_copy_and_csum_dev);
EXPORT_SYMBOL(skb_copy_expand);
EXPORT_SYMBOL(___pskb_trim);
EXPORT_SYMBOL(__pskb_pull_tail);
EXPORT_SYMBOL(pskb_expand_head);
EXPORT_SYMBOL(pskb_copy);
EXPORT_SYMBOL(skb_realloc_headroom);
EXPORT_SYMBOL(datagram_poll);
EXPORT_SYMBOL(put_cmsg);
EXPORT_SYMBOL(sock_kmalloc);
EXPORT_SYMBOL(sock_kfree_s);
EXPORT_SYMBOL(sock_map_fd);
EXPORT_SYMBOL(sockfd_lookup);

#ifdef CONFIG_FILTER
EXPORT_SYMBOL(sk_run_filter);
EXPORT_SYMBOL(sk_chk_filter);
#endif

EXPORT_SYMBOL(neigh_table_init);
EXPORT_SYMBOL(neigh_table_clear);
EXPORT_SYMBOL(neigh_resolve_output);
EXPORT_SYMBOL(neigh_connected_output);
EXPORT_SYMBOL(neigh_update);
EXPORT_SYMBOL(neigh_create);
EXPORT_SYMBOL(neigh_lookup);
EXPORT_SYMBOL(__neigh_event_send);
EXPORT_SYMBOL(neigh_event_ns);
EXPORT_SYMBOL(neigh_ifdown);
#ifdef CONFIG_ARPD
EXPORT_SYMBOL(neigh_app_ns);
#endif
#ifdef CONFIG_SYSCTL
EXPORT_SYMBOL(neigh_sysctl_register);
#endif
EXPORT_SYMBOL(pneigh_lookup);
EXPORT_SYMBOL(pneigh_enqueue);
EXPORT_SYMBOL(neigh_destroy);
EXPORT_SYMBOL(neigh_parms_alloc);
EXPORT_SYMBOL(neigh_parms_release);
EXPORT_SYMBOL(neigh_rand_reach_time);
EXPORT_SYMBOL(neigh_compat_output); 
EXPORT_SYMBOL(neigh_changeaddr);

/*	dst_entry	*/
EXPORT_SYMBOL(dst_alloc);
EXPORT_SYMBOL(__dst_free);
EXPORT_SYMBOL(dst_destroy);

/*	misc. support routines */
EXPORT_SYMBOL(net_ratelimit);
EXPORT_SYMBOL(net_random);
EXPORT_SYMBOL(net_srandom);

/* Needed by smbfs.o */
EXPORT_SYMBOL(__scm_destroy);
EXPORT_SYMBOL(__scm_send);

/* Needed by unix.o */
EXPORT_SYMBOL(scm_fp_dup);
EXPORT_SYMBOL(files_stat);
EXPORT_SYMBOL(memcpy_toiovec);

#ifdef CONFIG_IPX_MODULE
EXPORT_SYMBOL(make_8023_client);
EXPORT_SYMBOL(destroy_8023_client);
EXPORT_SYMBOL(make_EII_client);
EXPORT_SYMBOL(destroy_EII_client);
#endif

/* for 801q VLAN support */
#if defined(CONFIG_VLAN_8021Q) || defined(CONFIG_VLAN_8021Q_MODULE)
EXPORT_SYMBOL(dev_change_flags);
EXPORT_SYMBOL(vlan_ioctl_hook);
#endif

EXPORT_SYMBOL(sklist_destroy_socket);
EXPORT_SYMBOL(sklist_insert_socket);

EXPORT_SYMBOL(scm_detach_fds);

#if defined(CONFIG_BRIDGE) || defined(CONFIG_BRIDGE_MODULE)
EXPORT_SYMBOL(br_handle_frame_hook);
#ifdef CONFIG_INET
EXPORT_SYMBOL(br_ioctl_hook);
#endif
#endif

#ifdef CONFIG_NET_DIVERT
EXPORT_SYMBOL(alloc_divert_blk);
EXPORT_SYMBOL(free_divert_blk);
EXPORT_SYMBOL(divert_ioctl);
#endif /* CONFIG_NET_DIVERT */

#ifdef CONFIG_INET
/* Internet layer registration */
EXPORT_SYMBOL(inetdev_lock);
EXPORT_SYMBOL(inet_add_protocol);
EXPORT_SYMBOL(inet_del_protocol);
EXPORT_SYMBOL(inet_register_protosw);
EXPORT_SYMBOL(inet_unregister_protosw);
EXPORT_SYMBOL(ip_route_output_key);
EXPORT_SYMBOL(ip_route_input);
EXPORT_SYMBOL(icmp_send);
EXPORT_SYMBOL(icmp_statistics);
EXPORT_SYMBOL(icmp_err_convert);
EXPORT_SYMBOL(ip_options_compile);
EXPORT_SYMBOL(ip_options_undo);
EXPORT_SYMBOL(arp_create);
EXPORT_SYMBOL(arp_xmit);
EXPORT_SYMBOL(arp_send);
EXPORT_SYMBOL(arp_broken_ops);
EXPORT_SYMBOL(__ip_select_ident);
EXPORT_SYMBOL(ip_send_check);
EXPORT_SYMBOL(ip_fragment);
EXPORT_SYMBOL(inet_family_ops);
EXPORT_SYMBOL(in_aton);
EXPORT_SYMBOL(ip_mc_inc_group);
EXPORT_SYMBOL(ip_mc_dec_group);
EXPORT_SYMBOL(ip_mc_join_group);
EXPORT_SYMBOL(ip_finish_output);
EXPORT_SYMBOL(inet_stream_ops);
EXPORT_SYMBOL(inet_dgram_ops);
EXPORT_SYMBOL(ip_cmsg_recv);
EXPORT_SYMBOL(inet_addr_type); 
EXPORT_SYMBOL(inet_select_addr);
EXPORT_SYMBOL(ip_dev_find);
EXPORT_SYMBOL(inetdev_by_index);
EXPORT_SYMBOL(in_dev_finish_destroy);
EXPORT_SYMBOL(ip_defrag);

/* Route manipulation */
EXPORT_SYMBOL(ip_rt_ioctl);
EXPORT_SYMBOL(devinet_ioctl);
EXPORT_SYMBOL(register_inetaddr_notifier);
EXPORT_SYMBOL(unregister_inetaddr_notifier);

/* needed for ip_gre -cw */
EXPORT_SYMBOL(ip_statistics);

#ifdef CONFIG_DLCI_MODULE
extern int (*dlci_ioctl_hook)(unsigned int, void *);
EXPORT_SYMBOL(dlci_ioctl_hook);
#endif


#if defined (CONFIG_IPV6_MODULE) || defined (CONFIG_KHTTPD) || defined (CONFIG_KHTTPD_MODULE) || defined (CONFIG_IP_SCTP_MODULE)
/* inet functions common to v4 and v6 */
EXPORT_SYMBOL(inet_release);
EXPORT_SYMBOL(inet_stream_connect);
EXPORT_SYMBOL(inet_dgram_connect);
EXPORT_SYMBOL(inet_accept);
EXPORT_SYMBOL(inet_listen);
EXPORT_SYMBOL(inet_shutdown);
EXPORT_SYMBOL(inet_setsockopt);
EXPORT_SYMBOL(inet_getsockopt);
EXPORT_SYMBOL(inet_sendmsg);
EXPORT_SYMBOL(inet_recvmsg);
#ifdef INET_REFCNT_DEBUG
EXPORT_SYMBOL(inet_sock_nr);
#endif
EXPORT_SYMBOL(inet_sock_destruct);
EXPORT_SYMBOL(inet_sock_release);

/* Socket demultiplexing. */
EXPORT_SYMBOL(tcp_hashinfo);
EXPORT_SYMBOL(tcp_listen_wlock);
EXPORT_SYMBOL(udp_hash);
EXPORT_SYMBOL(udp_hash_lock);

EXPORT_SYMBOL(tcp_destroy_sock);
EXPORT_SYMBOL(ip_queue_xmit);
EXPORT_SYMBOL(memcpy_fromiovecend);
EXPORT_SYMBOL(csum_partial_copy_fromiovecend);
EXPORT_SYMBOL(tcp_v4_lookup_listener);
/* UDP/TCP exported functions for TCPv6 */
EXPORT_SYMBOL(udp_ioctl);
EXPORT_SYMBOL(udp_connect);
EXPORT_SYMBOL(udp_disconnect);
EXPORT_SYMBOL(udp_sendmsg);
EXPORT_SYMBOL(tcp_close);
EXPORT_SYMBOL(tcp_disconnect);
EXPORT_SYMBOL(tcp_accept);
EXPORT_SYMBOL(tcp_write_wakeup);
EXPORT_SYMBOL(tcp_write_space);
EXPORT_SYMBOL(tcp_poll);
EXPORT_SYMBOL(tcp_ioctl);
EXPORT_SYMBOL(tcp_shutdown);
EXPORT_SYMBOL(tcp_setsockopt);
EXPORT_SYMBOL(tcp_getsockopt);
EXPORT_SYMBOL(tcp_recvmsg);
EXPORT_SYMBOL(tcp_send_synack);
EXPORT_SYMBOL(tcp_check_req);
EXPORT_SYMBOL(tcp_child_process);
EXPORT_SYMBOL(tcp_parse_options);
EXPORT_SYMBOL(tcp_rcv_established);
EXPORT_SYMBOL(tcp_init_xmit_timers);
EXPORT_SYMBOL(tcp_clear_xmit_timers);
EXPORT_SYMBOL(tcp_statistics);
EXPORT_SYMBOL(tcp_rcv_state_process);
EXPORT_SYMBOL(tcp_timewait_state_process);
EXPORT_SYMBOL(tcp_timewait_cachep);
EXPORT_SYMBOL(tcp_timewait_kill);
EXPORT_SYMBOL(tcp_sendmsg);
EXPORT_SYMBOL(tcp_v4_rebuild_header);
EXPORT_SYMBOL(tcp_v4_send_check);
EXPORT_SYMBOL(tcp_v4_conn_request);
EXPORT_SYMBOL(tcp_create_openreq_child);
EXPORT_SYMBOL(tcp_bucket_create);
EXPORT_SYMBOL(__tcp_put_port);
EXPORT_SYMBOL(tcp_put_port);
EXPORT_SYMBOL(tcp_inherit_port);
EXPORT_SYMBOL(tcp_v4_syn_recv_sock);
EXPORT_SYMBOL(tcp_v4_do_rcv);
EXPORT_SYMBOL(tcp_v4_connect);
EXPORT_SYMBOL(tcp_unhash);
EXPORT_SYMBOL(udp_prot);
EXPORT_SYMBOL(tcp_prot);
EXPORT_SYMBOL(tcp_openreq_cachep);
EXPORT_SYMBOL(ipv4_specific);
EXPORT_SYMBOL(tcp_simple_retransmit);
EXPORT_SYMBOL(tcp_transmit_skb);
EXPORT_SYMBOL(tcp_connect);
EXPORT_SYMBOL(tcp_make_synack);
EXPORT_SYMBOL(tcp_tw_deschedule);
EXPORT_SYMBOL(tcp_delete_keepalive_timer);
EXPORT_SYMBOL(tcp_reset_keepalive_timer);
EXPORT_SYMBOL(sysctl_local_port_range);
EXPORT_SYMBOL(tcp_port_rover);
EXPORT_SYMBOL(udp_port_rover);
EXPORT_SYMBOL(tcp_sync_mss);
EXPORT_SYMBOL(net_statistics); 
EXPORT_SYMBOL(__tcp_mem_reclaim);
EXPORT_SYMBOL(tcp_sockets_allocated);
EXPORT_SYMBOL(sysctl_tcp_reordering);
EXPORT_SYMBOL(sysctl_tcp_rmem);
EXPORT_SYMBOL(sysctl_tcp_wmem);
EXPORT_SYMBOL(sysctl_tcp_ecn);
EXPORT_SYMBOL(tcp_cwnd_application_limited);
EXPORT_SYMBOL(tcp_sendpage);
EXPORT_SYMBOL(sysctl_tcp_low_latency);

EXPORT_SYMBOL(tcp_write_xmit);

EXPORT_SYMBOL(tcp_v4_remember_stamp); 

extern int sysctl_tcp_tw_recycle;

#ifdef CONFIG_SYSCTL
EXPORT_SYMBOL(sysctl_tcp_tw_recycle); 
EXPORT_SYMBOL(sysctl_max_syn_backlog);
#endif

#if defined (CONFIG_IPV6_MODULE)
EXPORT_SYMBOL(secure_tcpv6_sequence_number);
EXPORT_SYMBOL(secure_ipv6_id);
#endif

#endif

EXPORT_SYMBOL(tcp_read_sock);

#ifdef CONFIG_IP_SCTP_MODULE
EXPORT_SYMBOL(ip_setsockopt);
EXPORT_SYMBOL(ip_getsockopt);
EXPORT_SYMBOL(inet_ioctl);
EXPORT_SYMBOL(inet_bind);
EXPORT_SYMBOL(inet_getname);
#endif /* CONFIG_IP_SCTP_MODULE */

EXPORT_SYMBOL(netlink_set_err);
EXPORT_SYMBOL(netlink_broadcast);
EXPORT_SYMBOL(netlink_unicast);
EXPORT_SYMBOL(netlink_kernel_create);
EXPORT_SYMBOL(netlink_dump_start);
EXPORT_SYMBOL(netlink_ack);
EXPORT_SYMBOL(netlink_set_nonroot);
EXPORT_SYMBOL(netlink_register_notifier);
EXPORT_SYMBOL(netlink_unregister_notifier);
#if defined(CONFIG_NETLINK_DEV) || defined(CONFIG_NETLINK_DEV_MODULE)
EXPORT_SYMBOL(netlink_attach);
EXPORT_SYMBOL(netlink_detach);
EXPORT_SYMBOL(netlink_post);
#endif

EXPORT_SYMBOL(rtattr_parse);
EXPORT_SYMBOL(rtnetlink_links);
EXPORT_SYMBOL(__rta_fill);
EXPORT_SYMBOL(rtnetlink_dump_ifinfo);
EXPORT_SYMBOL(rtnetlink_put_metrics);
EXPORT_SYMBOL(rtnl);
EXPORT_SYMBOL(neigh_delete);
EXPORT_SYMBOL(neigh_add);
EXPORT_SYMBOL(neigh_dump_info);

EXPORT_SYMBOL(dev_set_allmulti);
EXPORT_SYMBOL(dev_set_promiscuity);
EXPORT_SYMBOL(sklist_remove_socket);
EXPORT_SYMBOL(rtnl_sem);
EXPORT_SYMBOL(rtnl_lock);
EXPORT_SYMBOL(rtnl_unlock);

/* ABI emulation layers need this */
EXPORT_SYMBOL(move_addr_to_kernel);
EXPORT_SYMBOL(move_addr_to_user);
                  
/* Used by at least ipip.c.  */
EXPORT_SYMBOL(ipv4_config);
EXPORT_SYMBOL(dev_open);

/* Used by other modules */
EXPORT_SYMBOL(xrlim_allow);

EXPORT_SYMBOL(ip_rcv);
EXPORT_SYMBOL(arp_rcv);
EXPORT_SYMBOL(arp_tbl);
#if defined(CONFIG_ATM_CLIP) || defined(CONFIG_ATM_CLIP_MODULE)
EXPORT_SYMBOL(clip_tbl_hook);
#endif
EXPORT_SYMBOL(arp_find);

#endif  /* CONFIG_INET */

#ifdef CONFIG_TR
EXPORT_SYMBOL(tr_type_trans);
#endif

/* Device callback registration */
EXPORT_SYMBOL(register_netdevice_notifier);
EXPORT_SYMBOL(unregister_netdevice_notifier);

/* support for loadable net drivers */
#ifdef CONFIG_NET
EXPORT_SYMBOL(loopback_dev);
EXPORT_SYMBOL(register_netdevice);
EXPORT_SYMBOL(unregister_netdevice);
EXPORT_SYMBOL(netdev_state_change);
EXPORT_SYMBOL(dev_new_index);
EXPORT_SYMBOL(dev_get_by_flags);
EXPORT_SYMBOL(__dev_get_by_flags);
EXPORT_SYMBOL(dev_get_by_index);
EXPORT_SYMBOL(__dev_get_by_index);
EXPORT_SYMBOL(dev_get_by_name);
EXPORT_SYMBOL(__dev_get_by_name);
EXPORT_SYMBOL(netdev_finish_unregister);
EXPORT_SYMBOL(netdev_set_master);
EXPORT_SYMBOL(eth_type_trans);
#ifdef CONFIG_FDDI
EXPORT_SYMBOL(fddi_type_trans);
#endif /* CONFIG_FDDI */
#if 0
EXPORT_SYMBOL(eth_copy_and_sum);
#endif
EXPORT_SYMBOL(alloc_skb);
EXPORT_SYMBOL(__kfree_skb);
EXPORT_SYMBOL(skb_clone);
EXPORT_SYMBOL(skb_copy);
EXPORT_SYMBOL(netif_rx);
EXPORT_SYMBOL(netif_receive_skb);
EXPORT_SYMBOL(dev_add_pack);
EXPORT_SYMBOL(dev_remove_pack);
EXPORT_SYMBOL(dev_get);
EXPORT_SYMBOL(dev_alloc);
EXPORT_SYMBOL(dev_alloc_name);
EXPORT_SYMBOL(__netdev_watchdog_up);
#ifdef CONFIG_KMOD
EXPORT_SYMBOL(dev_load);
#endif
EXPORT_SYMBOL(dev_ioctl);
EXPORT_SYMBOL(dev_queue_xmit);
#ifdef CONFIG_NET_HW_FLOWCONTROL
EXPORT_SYMBOL(netdev_dropping);
EXPORT_SYMBOL(netdev_register_fc);
EXPORT_SYMBOL(netdev_unregister_fc);
EXPORT_SYMBOL(netdev_fc_xoff);
#endif
EXPORT_SYMBOL(dev_base);
EXPORT_SYMBOL(dev_base_lock);
EXPORT_SYMBOL(dev_close);
EXPORT_SYMBOL(dev_mc_add);
EXPORT_SYMBOL(dev_mc_delete);
EXPORT_SYMBOL(dev_mc_upload);
EXPORT_SYMBOL(__kill_fasync);

EXPORT_SYMBOL(if_port_text);

#ifdef CONFIG_HIPPI
EXPORT_SYMBOL(hippi_type_trans);
#endif

#ifdef CONFIG_NET_FASTROUTE
EXPORT_SYMBOL(netdev_fastroute);
#endif

#ifdef CONFIG_SYSCTL
EXPORT_SYMBOL(sysctl_wmem_max);
EXPORT_SYMBOL(sysctl_rmem_max);
EXPORT_SYMBOL(sysctl_optmem_max);
#ifdef CONFIG_INET
EXPORT_SYMBOL(sysctl_ip_default_ttl);
#endif
#endif

/* Packet scheduler modules want these. */
EXPORT_SYMBOL(qdisc_destroy);
EXPORT_SYMBOL(qdisc_reset);
EXPORT_SYMBOL(qdisc_restart);
EXPORT_SYMBOL(qdisc_create_dflt);
EXPORT_SYMBOL(noop_qdisc);
EXPORT_SYMBOL(qdisc_tree_lock);
#ifdef CONFIG_NET_SCHED
PSCHED_EXPORTLIST;
EXPORT_SYMBOL(pfifo_qdisc_ops);
EXPORT_SYMBOL(bfifo_qdisc_ops);
EXPORT_SYMBOL(register_qdisc);
EXPORT_SYMBOL(unregister_qdisc);
EXPORT_SYMBOL(qdisc_get_rtab);
EXPORT_SYMBOL(qdisc_put_rtab);
EXPORT_SYMBOL(qdisc_copy_stats);
#ifdef CONFIG_NET_ESTIMATOR
EXPORT_SYMBOL(qdisc_new_estimator);
EXPORT_SYMBOL(qdisc_kill_estimator);
#endif
#ifdef CONFIG_NET_CLS_POLICE
EXPORT_SYMBOL(tcf_police);
EXPORT_SYMBOL(tcf_police_locate);
EXPORT_SYMBOL(tcf_police_destroy);
EXPORT_SYMBOL(tcf_police_dump);
#endif
#endif
#ifdef CONFIG_NET_CLS
EXPORT_SYMBOL(register_tcf_proto_ops);
EXPORT_SYMBOL(unregister_tcf_proto_ops);
#endif
#ifdef CONFIG_NETFILTER
#include <linux/netfilter.h>
EXPORT_SYMBOL(nf_register_hook);
EXPORT_SYMBOL(nf_unregister_hook);
EXPORT_SYMBOL(nf_register_sockopt);
EXPORT_SYMBOL(nf_unregister_sockopt);
EXPORT_SYMBOL(nf_reinject);
EXPORT_SYMBOL(nf_register_queue_handler);
EXPORT_SYMBOL(nf_unregister_queue_handler);
EXPORT_SYMBOL(nf_hook_slow);
EXPORT_SYMBOL(nf_hooks);
EXPORT_SYMBOL(nf_setsockopt);
EXPORT_SYMBOL(nf_getsockopt);
EXPORT_SYMBOL(ip_ct_attach);
#ifdef CONFIG_INET
#include <linux/netfilter_ipv4.h>
EXPORT_SYMBOL(ip_route_me_harder);
#endif
#endif

EXPORT_SYMBOL(register_gifconf);

EXPORT_SYMBOL(softnet_data);

#if defined(CONFIG_NET_RADIO) || defined(CONFIG_NET_PCMCIA_RADIO)
#include <net/iw_handler.h>
EXPORT_SYMBOL(wireless_send_event);
EXPORT_SYMBOL(iw_handler_set_spy);
EXPORT_SYMBOL(iw_handler_get_spy);
EXPORT_SYMBOL(iw_handler_set_thrspy);
EXPORT_SYMBOL(iw_handler_get_thrspy);
EXPORT_SYMBOL(wireless_spy_update);
#endif /* CONFIG_NET_RADIO || CONFIG_NET_PCMCIA_RADIO */

/* ethtool.c */
EXPORT_SYMBOL(ethtool_op_get_link);
EXPORT_SYMBOL(ethtool_op_get_tx_csum);
EXPORT_SYMBOL(ethtool_op_set_tx_csum);
EXPORT_SYMBOL(ethtool_op_get_sg);
EXPORT_SYMBOL(ethtool_op_set_sg);

#endif  /* CONFIG_NET */
