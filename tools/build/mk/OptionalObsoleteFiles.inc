#
# $FreeBSD$
#
# This file add support for the WITHOUT_* and WITH_* knobs in src.conf(5) to
# the check-old and delete-old* targets.
#

.if ${MK_ACPI} == no
OLD_FILES+=usr/sbin/acpiconf
OLD_FILES+=usr/sbin/acpidb
OLD_FILES+=usr/sbin/acpidump
OLD_FILES+=usr/sbin/iasl
OLD_FILES+=usr/share/man/man8/acpiconf.8.gz
OLD_FILES+=usr/share/man/man8/acpidb.8.gz
OLD_FILES+=usr/share/man/man8/acpidump.8.gz
OLD_FILES+=usr/share/man/man8/iasl.8.gz
.endif

#.if ${MK_ATM} == no
# to be filled in
#.endif

.if ${MK_AUDIT} == no
OLD_FILES+=usr/sbin/audit
OLD_FILES+=usr/sbin/auditd
OLD_FILES+=usr/sbin/auditreduce
OLD_FILES+=usr/sbin/praudit
OLD_FILES+=usr/share/man/man1/auditreduce.1.gz
OLD_FILES+=usr/share/man/man1/praudit.1.gz
OLD_FILES+=usr/share/man/man8/audit.8.gz
OLD_FILES+=usr/share/man/man8/auditd.8.gz
.endif

#.if ${MK_AUTHPF} == no
# to be filled in
#.endif

.if ${MK_BIND} == no
OLD_FILES+=usr/bin/dig
OLD_FILES+=usr/bin/host
OLD_FILES+=usr/bin/nslookup
OLD_FILES+=usr/bin/nsupdate
OLD_FILES+=usr/include/lwres/context.h
OLD_FILES+=usr/include/lwres/int.h
OLD_FILES+=usr/include/lwres/ipv6.h
OLD_FILES+=usr/include/lwres/lang.h
OLD_FILES+=usr/include/lwres/list.h
OLD_FILES+=usr/include/lwres/lwbuffer.h
OLD_FILES+=usr/include/lwres/lwpacket.h
OLD_FILES+=usr/include/lwres/lwres.h
OLD_FILES+=usr/include/lwres/result.h
OLD_FILES+=usr/include/lwres/version.h
OLD_FILES+=usr/include/lwres/net.h
OLD_FILES+=usr/include/lwres/netdb.h
OLD_FILES+=usr/include/lwres/platform.h
OLD_DIRS+=usr/include/lwres
OLD_FILES+=usr/lib/liblwres.a
OLD_FILES+=usr/lib/liblwres_p.a
OLD_LIBS+=usr/lib/liblwres.so.10
OLD_FILES+=usr/lib/liblwres.so
OLD_FILES+=usr/sbin/named
OLD_FILES+=usr/sbin/lwresd
OLD_FILES+=usr/sbin/named-checkconf
OLD_FILES+=usr/sbin/named-checkzone
OLD_FILES+=usr/sbin/named.reload
OLD_FILES+=usr/sbin/named.reconfig
OLD_FILES+=usr/sbin/rndc
OLD_FILES+=usr/sbin/rndc-confgen
OLD_FILES+=usr/share/doc/bind9/arm/Bv9ARM.ch01.html
OLD_FILES+=usr/share/doc/bind9/arm/Bv9ARM.ch02.html
OLD_FILES+=usr/share/doc/bind9/arm/Bv9ARM.ch03.html
OLD_FILES+=usr/share/doc/bind9/arm/Bv9ARM.ch04.html
OLD_FILES+=usr/share/doc/bind9/arm/Bv9ARM.ch05.html
OLD_FILES+=usr/share/doc/bind9/arm/Bv9ARM.ch06.html
OLD_FILES+=usr/share/doc/bind9/arm/Bv9ARM.ch07.html
OLD_FILES+=usr/share/doc/bind9/arm/Bv9ARM.ch08.html
OLD_FILES+=usr/share/doc/bind9/arm/Bv9ARM.ch09.html
OLD_FILES+=usr/share/doc/bind9/arm/Bv9ARM.html
OLD_DIRS+=usr/share/doc/bind9/arm
OLD_FILES+=usr/share/doc/bind9/misc/dnssec
OLD_FILES+=usr/share/doc/bind9/misc/format-options.pl
OLD_FILES+=usr/share/doc/bind9/misc/ipv6
OLD_FILES+=usr/share/doc/bind9/misc/migration
OLD_FILES+=usr/share/doc/bind9/misc/migration-4to9
OLD_FILES+=usr/share/doc/bind9/misc/options
OLD_FILES+=usr/share/doc/bind9/misc/rfc-compliance
OLD_FILES+=usr/share/doc/bind9/misc/roadmap
OLD_FILES+=usr/share/doc/bind9/misc/sdb
OLD_DIRS+=usr/share/doc/bind9/misc
OLD_DIRS+=usr/share/doc/bind9
OLD_FILES+=usr/share/doc/bind9/CHANGES
OLD_FILES+=usr/share/doc/bind9/COPYRIGHT
OLD_FILES+=usr/share/doc/bind9/FAQ
OLD_FILES+=usr/share/doc/bind9/README
OLD_FILES+=usr/share/man/man1/dig.1.gz
OLD_FILES+=usr/share/man/man1/host.1.gz
OLD_FILES+=usr/share/man/man1/nslookup.1.gz
OLD_FILES+=usr/share/man/man3/lwres.3.gz
OLD_FILES+=usr/share/man/man3/lwres_buffer.3.gz
OLD_FILES+=usr/share/man/man3/lwres_config.3.gz
OLD_FILES+=usr/share/man/man3/lwres_context.3.gz
OLD_FILES+=usr/share/man/man3/lwres_gabn.3.gz
OLD_FILES+=usr/share/man/man3/lwres_gai_strerror.3.gz
OLD_FILES+=usr/share/man/man3/lwres_getaddrinfo.3.gz
OLD_FILES+=usr/share/man/man3/lwres_gethostent.3.gz
OLD_FILES+=usr/share/man/man3/lwres_getipnode.3.gz
OLD_FILES+=usr/share/man/man3/lwres_getnameinfo.3.gz
OLD_FILES+=usr/share/man/man3/lwres_getrrsetbyname.3.gz
OLD_FILES+=usr/share/man/man3/lwres_gnba.3.gz
OLD_FILES+=usr/share/man/man3/lwres_inetntop.3.gz
OLD_FILES+=usr/share/man/man3/lwres_hstrerror.3.gz
OLD_FILES+=usr/share/man/man3/lwres_noop.3.gz
OLD_FILES+=usr/share/man/man3/lwres_packet.3.gz
OLD_FILES+=usr/share/man/man3/lwres_resutil.3.gz
OLD_FILES+=usr/share/man/man3/lwres_buffer_add.3.gz
OLD_FILES+=usr/share/man/man3/lwres_buffer_back.3.gz
OLD_FILES+=usr/share/man/man3/lwres_buffer_clear.3.gz
OLD_FILES+=usr/share/man/man3/lwres_buffer_first.3.gz
OLD_FILES+=usr/share/man/man3/lwres_buffer_forward.3.gz
OLD_FILES+=usr/share/man/man3/lwres_buffer_getmem.3.gz
OLD_FILES+=usr/share/man/man3/lwres_buffer_getuint16.3.gz
OLD_FILES+=usr/share/man/man3/lwres_buffer_getuint32.3.gz
OLD_FILES+=usr/share/man/man3/lwres_buffer_getuint8.3.gz
OLD_FILES+=usr/share/man/man3/lwres_buffer_init.3.gz
OLD_FILES+=usr/share/man/man3/lwres_buffer_invalidate.3.gz
OLD_FILES+=usr/share/man/man3/lwres_buffer_putmem.3.gz
OLD_FILES+=usr/share/man/man3/lwres_buffer_putuint16.3.gz
OLD_FILES+=usr/share/man/man3/lwres_buffer_putuint32.3.gz
OLD_FILES+=usr/share/man/man3/lwres_buffer_putuint8.3.gz
OLD_FILES+=usr/share/man/man3/lwres_buffer_subtract.3.gz
OLD_FILES+=usr/share/man/man3/lwres_conf_clear.3.gz
OLD_FILES+=usr/share/man/man3/lwres_conf_get.3.gz
OLD_FILES+=usr/share/man/man3/lwres_conf_init.3.gz
OLD_FILES+=usr/share/man/man3/lwres_conf_parse.3.gz
OLD_FILES+=usr/share/man/man3/lwres_conf_print.3.gz
OLD_FILES+=usr/share/man/man3/lwres_context_allocmem.3.gz
OLD_FILES+=usr/share/man/man3/lwres_context_create.3.gz
OLD_FILES+=usr/share/man/man3/lwres_context_destroy.3.gz
OLD_FILES+=usr/share/man/man3/lwres_context_freemem.3.gz
OLD_FILES+=usr/share/man/man3/lwres_freeaddrinfo.3.gz
OLD_FILES+=usr/share/man/man3/lwres_context_initserial.3.gz
OLD_FILES+=usr/share/man/man3/lwres_context_nextserial.3.gz
OLD_FILES+=usr/share/man/man3/lwres_context_sendrecv.3.gz
OLD_FILES+=usr/share/man/man3/lwres_gabnrequest_free.3.gz
OLD_FILES+=usr/share/man/man3/lwres_gabnrequest_parse.3.gz
OLD_FILES+=usr/share/man/man3/lwres_gabnrequest_render.3.gz
OLD_FILES+=usr/share/man/man3/lwres_gabnresponse_free.3.gz
OLD_FILES+=usr/share/man/man3/lwres_gabnresponse_parse.3.gz
OLD_FILES+=usr/share/man/man3/lwres_gabnresponse_render.3.gz
OLD_FILES+=usr/share/man/man3/lwres_endhostent.3.gz
OLD_FILES+=usr/share/man/man3/lwres_endhostent_r.3.gz
OLD_FILES+=usr/share/man/man3/lwres_gethostbyaddr.3.gz
OLD_FILES+=usr/share/man/man3/lwres_gethostbyaddr_r.3.gz
OLD_FILES+=usr/share/man/man3/lwres_gethostbyname.3.gz
OLD_FILES+=usr/share/man/man3/lwres_gethostbyname2.3.gz
OLD_FILES+=usr/share/man/man3/lwres_gethostbyname_r.3.gz
OLD_FILES+=usr/share/man/man3/lwres_gethostent_r.3.gz
OLD_FILES+=usr/share/man/man3/lwres_sethostent.3.gz
OLD_FILES+=usr/share/man/man3/lwres_sethostent_r.3.gz
OLD_FILES+=usr/share/man/man3/lwres_freehostent.3.gz
OLD_FILES+=usr/share/man/man3/lwres_getipnodebyaddr.3.gz
OLD_FILES+=usr/share/man/man3/lwres_getipnodebyname.3.gz
OLD_FILES+=usr/share/man/man3/lwres_gnbarequest_free.3.gz
OLD_FILES+=usr/share/man/man3/lwres_gnbarequest_parse.3.gz
OLD_FILES+=usr/share/man/man3/lwres_gnbarequest_render.3.gz
OLD_FILES+=usr/share/man/man3/lwres_gnbaresponse_free.3.gz
OLD_FILES+=usr/share/man/man3/lwres_gnbaresponse_parse.3.gz
OLD_FILES+=usr/share/man/man3/lwres_gnbaresponse_render.3.gz
OLD_FILES+=usr/share/man/man3/lwres_herror.3.gz
OLD_FILES+=usr/share/man/man3/lwres_net_ntop.3.gz
OLD_FILES+=usr/share/man/man3/lwres_nooprequest_free.3.gz
OLD_FILES+=usr/share/man/man3/lwres_nooprequest_parse.3.gz
OLD_FILES+=usr/share/man/man3/lwres_nooprequest_render.3.gz
OLD_FILES+=usr/share/man/man3/lwres_noopresponse_free.3.gz
OLD_FILES+=usr/share/man/man3/lwres_noopresponse_parse.3.gz
OLD_FILES+=usr/share/man/man3/lwres_noopresponse_render.3.gz
OLD_FILES+=usr/share/man/man3/lwres_lwpacket_parseheader.3.gz
OLD_FILES+=usr/share/man/man3/lwres_lwpacket_renderheader.3.gz
OLD_FILES+=usr/share/man/man3/lwres_addr_parse.3.gz
OLD_FILES+=usr/share/man/man3/lwres_getaddrsbyname.3.gz
OLD_FILES+=usr/share/man/man3/lwres_getnamebyaddr.3.gz
OLD_FILES+=usr/share/man/man3/lwres_string_parse.3.gz
OLD_FILES+=usr/share/man/man5/named.conf.5.gz
OLD_FILES+=usr/share/man/man5/rndc.conf.5.gz
OLD_FILES+=usr/share/man/man8/nsupdate.8.gz
OLD_FILES+=usr/share/man/man8/dnssec-keygen.8.gz
OLD_FILES+=usr/share/man/man8/dnssec-signzone.8.gz
OLD_FILES+=usr/share/man/man8/named.8.gz
OLD_FILES+=usr/share/man/man8/lwresd.8.gz
OLD_FILES+=usr/share/man/man8/named-checkconf.8.gz
OLD_FILES+=usr/share/man/man8/named-checkzone.8.gz
OLD_FILES+=usr/share/man/man8/named.reload.8.gz
OLD_FILES+=usr/share/man/man8/named.reconfig.8.gz
OLD_FILES+=usr/share/man/man8/rndc.8.gz
OLD_FILES+=usr/share/man/man8/rndc-confgen.8.gz
OLD_DIRS+=var/named/dev
OLD_DIRS+=var/named/var/stats
OLD_DIRS+=var/named/var/run/named
OLD_DIRS+=var/named/var/run
OLD_DIRS+=var/named/var/log
OLD_DIRS+=var/named/var/dump
OLD_DIRS+=var/named/var
.endif

.if ${MK_BIND_DNSSEC} == no || ${MK_BIND} == no
OLD_FILES+=usr/sbin/dnssec-keygen
OLD_FILES+=usr/sbin/dnssec-signzone
.endif

.if ${MK_BIND_ETC} == no || ${MK_BIND} == no
OLD_FILES+=var/named/etc/namedb/PROTO.localhost-v6.rev
OLD_FILES+=var/named/etc/namedb/PROTO.localhost.rev
OLD_FILES+=var/named/etc/namedb/make-localhost
#OLD_FILES+=var/named/etc/namedb/named.conf	# intentionally left out
OLD_FILES+=var/named/etc/namedb/named.root
OLD_DIRS+=var/named/etc/namedb/slave
OLD_DIRS+=var/named/etc/namedb/master
OLD_DIRS+=var/named/etc/namedb/dynamic
#OLD_DIRS+=var/named/etc/namedb
#OLD_DIRS+=var/named/etc
.endif

#.if ${MK_BIND_LIBS} == no || ${MK_BIND} == no
# to be filled in and removed above
#.endif

#.if ${MK_BIND_LIBS_LWRES} == no || ${MK_BIND} == no
# to be filled in and removed above
#.endif

#.if ${MK_BIND_NAMED} == no || ${MK_BIND} == no
# to be filled in and removed above
#.endif

.if ${MK_BLUETOOTH} == no
OLD_FILES+=usr/bin/bthost
OLD_FILES+=usr/bin/btsockstat
OLD_FILES+=usr/bin/rfcomm_sppd
OLD_FILES+=usr/include/netgraph/bluetooth/include/ng_bluetooth.h
OLD_FILES+=usr/include/netgraph/bluetooth/include/ng_bt3c.h
OLD_FILES+=usr/include/netgraph/bluetooth/include/ng_btsocket.h
OLD_FILES+=usr/include/netgraph/bluetooth/include/ng_btsocket_hci_raw.h
OLD_FILES+=usr/include/netgraph/bluetooth/include/ng_btsocket_l2cap.h
OLD_FILES+=usr/include/netgraph/bluetooth/include/ng_btsocket_rfcomm.h
OLD_FILES+=usr/include/netgraph/bluetooth/include/ng_h4.h
OLD_FILES+=usr/include/netgraph/bluetooth/include/ng_hci.h
OLD_FILES+=usr/include/netgraph/bluetooth/include/ng_l2cap.h
OLD_FILES+=usr/include/netgraph/bluetooth/include/ng_ubt.h
OLD_DIRS+=usr/include/netgraph/bluetooth/include
OLD_DIRS+=usr/include/netgraph/bluetooth
OLD_FILES+=usr/include/bluetooth.h
OLD_FILES+=usr/include/sdp.h
OLD_FILES+=usr/lib/libbluetooth.a
OLD_FILES+=usr/lib/libbluetooth_p.a
OLD_LIBS+=usr/lib/libbluetooth.so.2
OLD_FILES+=usr/lib/libbluetooth.so
OLD_FILES+=usr/lib/libsdp.a
OLD_FILES+=usr/lib/libsdp_p.a
OLD_LIBS+=usr/lib/libsdp.so.2
OLD_FILES+=usr/lib/libsdp.so
OLD_FILES+=usr/sbin/bcmfw
OLD_FILES+=usr/sbin/bt3cfw
OLD_FILES+=usr/sbin/hccontrol
OLD_FILES+=usr/sbin/hcsecd
OLD_FILES+=usr/sbin/hcseriald
OLD_FILES+=usr/sbin/l2control
OLD_FILES+=usr/sbin/l2ping
OLD_FILES+=usr/sbin/rfcomm_pppd
OLD_FILES+=usr/sbin/sdpcontrol
OLD_FILES+=usr/sbin/sdpd
OLD_FILES+=usr/share/man/man1/bthost.1.gz
OLD_FILES+=usr/share/man/man1/btsockstat.1.gz
OLD_FILES+=usr/share/man/man1/rfcomm_sppd.1.gz
OLD_FILES+=usr/share/man/man3/bluetooth.3.gz
OLD_FILES+=usr/share/man/man3/bt_gethostbyname.3.gz
OLD_FILES+=usr/share/man/man3/bt_gethostbyaddr.3.gz
OLD_FILES+=usr/share/man/man3/bt_gethostent.3.gz
OLD_FILES+=usr/share/man/man3/bt_sethostent.3.gz
OLD_FILES+=usr/share/man/man3/bt_endhostent.3.gz
OLD_FILES+=usr/share/man/man3/bt_getprotobyname.3.gz
OLD_FILES+=usr/share/man/man3/bt_getprotobynumber.3.gz
OLD_FILES+=usr/share/man/man3/bt_getprotoent.3.gz
OLD_FILES+=usr/share/man/man3/bt_setprotoent.3.gz
OLD_FILES+=usr/share/man/man3/bt_endprotoent.3.gz
OLD_FILES+=usr/share/man/man3/bt_ntoa.3.gz
OLD_FILES+=usr/share/man/man3/bt_aton.3.gz
OLD_FILES+=usr/share/man/man3/sdp.3.gz
OLD_FILES+=usr/share/man/man3/SDP_GET8.3.gz
OLD_FILES+=usr/share/man/man3/SDP_GET16.3.gz
OLD_FILES+=usr/share/man/man3/SDP_GET32.3.gz
OLD_FILES+=usr/share/man/man3/SDP_GET64.3.gz
OLD_FILES+=usr/share/man/man3/SDP_GET128.3.gz
OLD_FILES+=usr/share/man/man3/SDP_PUT8.3.gz
OLD_FILES+=usr/share/man/man3/SDP_PUT16.3.gz
OLD_FILES+=usr/share/man/man3/SDP_PUT32.3.gz
OLD_FILES+=usr/share/man/man3/SDP_PUT64.3.gz
OLD_FILES+=usr/share/man/man3/SDP_PUT128.3.gz
OLD_FILES+=usr/share/man/man3/sdp_open.3.gz
OLD_FILES+=usr/share/man/man3/sdp_open_local.3.gz
OLD_FILES+=usr/share/man/man3/sdp_close.3.gz
OLD_FILES+=usr/share/man/man3/sdp_error.3.gz
OLD_FILES+=usr/share/man/man3/sdp_search.3.gz
OLD_FILES+=usr/share/man/man3/sdp_attr2desc.3.gz
OLD_FILES+=usr/share/man/man3/sdp_uuid2desc.3.gz
OLD_FILES+=usr/share/man/man3/sdp_register_service.3.gz
OLD_FILES+=usr/share/man/man3/sdp_unregister_service.3.gz
OLD_FILES+=usr/share/man/man3/sdp_change_service.3.gz
OLD_FILES+=usr/share/man/man5/hcsecd.conf.5.gz
OLD_FILES+=usr/share/man/man8/bcmfw.8.gz
OLD_FILES+=usr/share/man/man8/bt3cfw.8.gz
OLD_FILES+=usr/share/man/man8/hcsecd.8.gz
OLD_FILES+=usr/share/man/man8/hccontrol.8.gz
OLD_FILES+=usr/share/man/man8/hcseriald.8.gz
OLD_FILES+=usr/share/man/man8/l2control.8.gz
OLD_FILES+=usr/share/man/man8/l2ping.8.gz
OLD_FILES+=usr/share/man/man8/rfcomm_pppd.8.gz
OLD_FILES+=usr/share/man/man8/sdpcontrol.8.gz
OLD_FILES+=usr/share/man/man8/sdpd.8.gz
.endif

#.if ${MK_BOOT} == no
# to be filled in
#.endif

#.if ${MK_CALENDAR} == no
# to be filled in
#.endif

#.if ${MK_CPP} == no
# to be filled in
#.endif

#.if ${MK_CRYPT} == no
# to be filled in
#.endif

#.if ${MK_CVS} == no
# to be filled in
#.endif

#.if ${MK_CXX} == no
# to be filled in
#.endif

#.if ${MK_DICT} == no
# to be filled in
#.endif

#.if ${MK_EXAMPLES} == no
# to be filled in
#.endif

.if ${MK_FORTRAN} == no
OLD_FILES+=usr/bin/f77
OLD_FILES+=usr/include/g2c.h
OLD_FILES+=usr/lib/libg2c.a
OLD_FILES+=usr/lib/libg2c_p.a
OLD_FILES+=usr/lib/libg2c.so.2
OLD_FILES+=usr/lib/libg2c.so
OLD_FILES+=usr/libexec/f771
OLD_FILES+=usr/share/info/g77.info.gz
OLD_FILES+=usr/share/man/man1/f77.1.gz
.endif

#.if ${MK_GAMES} == no
# to be filled in
#.endif

#.if ${MK_GCOV} == no
# to be filled in
#.endif

#.if ${MK_GDB} == no
# to be filled in
#.endif

#.if ${MK_GPIB} == no
# none
#.endif

#.if ${MK_GROFF} == no
# to be filled in
#.endif

#.if ${MK_HESIOD} == no
# to be filled in
#.endif

#.if ${MK_HTML} == no
# to be filled in
#.endif

.if ${MK_I4B} == no
OLD_FILES+=usr/sbin/dtmfdecode
OLD_FILES+=usr/sbin/g711conv
OLD_FILES+=usr/sbin/isdnd
OLD_FILES+=usr/sbin/isdndebug
OLD_FILES+=usr/sbin/isdndecode
OLD_FILES+=usr/sbin/isdnmonitor
OLD_FILES+=usr/sbin/isdnphone
OLD_FILES+=usr/sbin/isdntel
OLD_FILES+=usr/sbin/isdntelctl
OLD_FILES+=usr/sbin/isdntrace
OLD_FILES+=usr/share/isdn/0.al
OLD_FILES+=usr/share/isdn/1.al
OLD_FILES+=usr/share/isdn/2.al
OLD_FILES+=usr/share/isdn/3.al
OLD_FILES+=usr/share/isdn/4.al
OLD_FILES+=usr/share/isdn/5.al
OLD_FILES+=usr/share/isdn/6.al
OLD_FILES+=usr/share/isdn/7.al
OLD_FILES+=usr/share/isdn/8.al
OLD_FILES+=usr/share/isdn/9.al
OLD_FILES+=usr/share/isdn/beep.al
OLD_FILES+=usr/share/isdn/msg.al
OLD_FILES+=usr/share/man/man1/dtmfdecode.1.gz
OLD_FILES+=usr/share/man/man1/g711conv.1.gz
OLD_FILES+=usr/share/man/man4/i4b.4.gz
OLD_FILES+=usr/share/man/man4/i4bcapi.4.gz
OLD_FILES+=usr/share/man/man4/i4bctl.4.gz
OLD_FILES+=usr/share/man/man4/i4bing.4.gz
OLD_FILES+=usr/share/man/man4/i4bipr.4.gz
OLD_FILES+=usr/share/man/man4/i4bisppp.4.gz
OLD_FILES+=usr/share/man/man4/i4bq921.4.gz
OLD_FILES+=usr/share/man/man4/i4bq931.4.gz
OLD_FILES+=usr/share/man/man4/i4brbch.4.gz
OLD_FILES+=usr/share/man/man4/i4btel.4.gz
OLD_FILES+=usr/share/man/man4/i4btrc.4.gz
OLD_FILES+=usr/share/man/man4/iavc.4.gz
OLD_FILES+=usr/share/man/man4/isic.4.gz
OLD_FILES+=usr/share/man/man4/ifpi.4.gz
OLD_FILES+=usr/share/man/man4/ifpi2.4.gz
OLD_FILES+=usr/share/man/man4/ifpnp.4.gz
OLD_FILES+=usr/share/man/man4/ihfc.4.gz
OLD_FILES+=usr/share/man/man4/itjc.4.gz
OLD_FILES+=usr/share/man/man4/iwic.4.gz
OLD_FILES+=usr/share/man/man5/isdnd.rc.5.gz
OLD_FILES+=usr/share/man/man5/isdnd.rates.5.gz
OLD_FILES+=usr/share/man/man5/isdnd.acct.5.gz
OLD_FILES+=usr/share/man/man8/isdnd.8.gz
OLD_FILES+=usr/share/man/man8/isdndebug.8.gz
OLD_FILES+=usr/share/man/man8/isdndecode.8.gz
OLD_FILES+=usr/share/man/man8/isdnmonitor.8.gz
OLD_FILES+=usr/share/man/man8/isdnphone.8.gz
OLD_FILES+=usr/share/man/man8/isdntel.8.gz
OLD_FILES+=usr/share/man/man8/isdntelctl.8.gz
OLD_FILES+=usr/share/man/man8/isdntrace.8.gz
.endif

.if ${MK_INET6} == no
OLD_FILES+=rescue/ping6
OLD_FILES+=sbin/ping6
OLD_FILES+=sbin/rtsol
OLD_FILES+=usr/sbin/faithd
OLD_FILES+=usr/sbin/ip6addrctl
OLD_FILES+=usr/sbin/mld6query
OLD_FILES+=usr/sbin/ndp
OLD_FILES+=usr/sbin/rip6query
OLD_FILES+=usr/sbin/route6d
OLD_FILES+=usr/sbin/rrenumd
OLD_FILES+=usr/sbin/rtadvd
OLD_FILES+=usr/sbin/rtsold
OLD_FILES+=usr/sbin/traceroute6
OLD_FILES+=usr/share/man/man5/rrenumd.conf.5.gz
OLD_FILES+=usr/share/man/man5/rtadvd.conf.5.gz
OLD_FILES+=usr/share/man/man8/ip6addrctl.8.gz
OLD_FILES+=usr/share/man/man8/mld6query.8.gz
OLD_FILES+=usr/share/man/man8/ndp.8.gz
OLD_FILES+=usr/share/man/man8/ping6.8.gz
OLD_FILES+=usr/share/man/man8/rip6query.8.gz
OLD_FILES+=usr/share/man/man8/route6d.8.gz
OLD_FILES+=usr/share/man/man8/rrenumd.8.gz
OLD_FILES+=usr/share/man/man8/rtadvd.8.gz
OLD_FILES+=usr/share/man/man8/rtsol.8.gz
OLD_FILES+=usr/share/man/man8/rtsold.8.gz
OLD_FILES+=usr/share/man/man8/traceroute6.8.gz
.endif

#.if ${MK_INFO} == no
# to be filled in
#.endif

.if ${MK_IPFILTER} == no
OLD_FILES+=rescue/ipf
OLD_FILES+=sbin/ipf
OLD_FILES+=sbin/ipfs
OLD_FILES+=sbin/ipfstat
OLD_FILES+=sbin/ipftest
OLD_FILES+=sbin/ipmon
OLD_FILES+=sbin/ipnat
OLD_FILES+=sbin/ippool
OLD_FILES+=sbin/ipresend
OLD_FILES+=usr/include/netinet/ip_auth.h
OLD_FILES+=usr/include/netinet/ip_compat.h
OLD_FILES+=usr/include/netinet/ip_fil.h
OLD_FILES+=usr/include/netinet/ip_frag.h
OLD_FILES+=usr/include/netinet/ip_htable.h
OLD_FILES+=usr/include/netinet/ip_lookup.h
OLD_FILES+=usr/include/netinet/ip_nat.h
OLD_FILES+=usr/include/netinet/ip_pool.h
OLD_FILES+=usr/include/netinet/ip_proxy.h
OLD_FILES+=usr/include/netinet/ip_rules.h
OLD_FILES+=usr/include/netinet/ip_scan.h
OLD_FILES+=usr/include/netinet/ip_state.h
OLD_FILES+=usr/include/netinet/ip_sync.h
OLD_FILES+=usr/include/netinet/ipl.h
OLD_FILES+=usr/share/examples/ipfilter/README
OLD_FILES+=usr/share/examples/ipfilter/BASIC.NAT
OLD_FILES+=usr/share/examples/ipfilter/BASIC_1.FW
OLD_FILES+=usr/share/examples/ipfilter/BASIC_2.FW
OLD_FILES+=usr/share/examples/ipfilter/example.1
OLD_FILES+=usr/share/examples/ipfilter/example.2
OLD_FILES+=usr/share/examples/ipfilter/example.3
OLD_FILES+=usr/share/examples/ipfilter/example.4
OLD_FILES+=usr/share/examples/ipfilter/example.5
OLD_FILES+=usr/share/examples/ipfilter/example.6
OLD_FILES+=usr/share/examples/ipfilter/example.7
OLD_FILES+=usr/share/examples/ipfilter/example.8
OLD_FILES+=usr/share/examples/ipfilter/example.9
OLD_FILES+=usr/share/examples/ipfilter/example.10
OLD_FILES+=usr/share/examples/ipfilter/example.11
OLD_FILES+=usr/share/examples/ipfilter/example.12
OLD_FILES+=usr/share/examples/ipfilter/example.13
OLD_FILES+=usr/share/examples/ipfilter/example.sr
OLD_FILES+=usr/share/examples/ipfilter/firewall
OLD_FILES+=usr/share/examples/ipfilter/ftp-proxy
OLD_FILES+=usr/share/examples/ipfilter/ftppxy
OLD_FILES+=usr/share/examples/ipfilter/nat-setup
OLD_FILES+=usr/share/examples/ipfilter/nat.eg
OLD_FILES+=usr/share/examples/ipfilter/server
OLD_FILES+=usr/share/examples/ipfilter/tcpstate
OLD_FILES+=usr/share/examples/ipfilter/example.14
OLD_FILES+=usr/share/examples/ipfilter/firewall.1
OLD_FILES+=usr/share/examples/ipfilter/firewall.2
OLD_FILES+=usr/share/examples/ipfilter/ipf.conf.permissive
OLD_FILES+=usr/share/examples/ipfilter/ipf.conf.restrictive
OLD_FILES+=usr/share/examples/ipfilter/ipf.conf.sample
OLD_FILES+=usr/share/examples/ipfilter/ipnat.conf.sample
OLD_FILES+=usr/share/examples/ipfilter/ipf-howto.txt
OLD_FILES+=usr/share/examples/ipfilter/examples.txt
OLD_FILES+=usr/share/examples/ipfilter/rules.txt
OLD_FILES+=usr/share/man/man1/ipftest.1.gz
OLD_FILES+=usr/share/man/man1/ipresend.1.gz
OLD_FILES+=usr/share/man/man4/ipf.4.gz
OLD_FILES+=usr/share/man/man4/ipl.4.gz
OLD_FILES+=usr/share/man/man4/ipfilter.4.gz
OLD_FILES+=usr/share/man/man4/ipnat.4.gz
OLD_FILES+=usr/share/man/man5/ipf.5.gz
OLD_FILES+=usr/share/man/man5/ipf.conf.5.gz
OLD_FILES+=usr/share/man/man5/ipf6.conf.5.gz
OLD_FILES+=usr/share/man/man5/ipnat.5.gz
OLD_FILES+=usr/share/man/man5/ipnat.conf.5.gz
OLD_FILES+=usr/share/man/man5/ippool.5.gz
OLD_FILES+=usr/share/man/man8/ipf.8.gz
OLD_FILES+=usr/share/man/man8/ipfs.8.gz
OLD_FILES+=usr/share/man/man8/ipfstat.8.gz
OLD_FILES+=usr/share/man/man8/ipmon.8.gz
OLD_FILES+=usr/share/man/man8/ipnat.8.gz
OLD_FILES+=usr/share/man/man8/ippool.8.gz
.endif

#.if ${MK_IPX} == no
# to be filled in
#.endif

.if ${MK_KERBEROS} == no
OLD_FILES+=usr/bin/ksu
OLD_FILES+=usr/bin/kadmin
OLD_FILES+=usr/bin/kdestroy
OLD_FILES+=usr/bin/kinit
OLD_FILES+=usr/bin/klist
OLD_FILES+=usr/bin/kpasswd
OLD_FILES+=usr/bin/krb5-config
OLD_FILES+=usr/bin/verify_krb5_conf
OLD_FILES+=usr/include/kadm5/admin.h
OLD_FILES+=usr/include/kadm5/kadm5-private.h
OLD_FILES+=usr/include/kadm5/kadm5-protos.h
OLD_FILES+=usr/include/kadm5/kadm5_err.h
OLD_FILES+=usr/include/kadm5/private.h
OLD_FILES+=usr/include/asn1_err.h
OLD_FILES+=usr/include/krb5_asn1.h
OLD_FILES+=usr/include/hdb.h
OLD_FILES+=usr/include/hdb-private.h
OLD_FILES+=usr/include/hdb-protos.h
OLD_FILES+=usr/include/hdb_asn1.h
OLD_FILES+=usr/include/hdb_err.h
OLD_FILES+=usr/include/kafs.h
OLD_FILES+=usr/include/heim_err.h
OLD_FILES+=usr/include/k524_err.h
OLD_FILES+=usr/include/krb5-protos.h
OLD_FILES+=usr/include/krb5-types.h
OLD_FILES+=usr/include/krb5.h
OLD_FILES+=usr/include/krb5_err.h
OLD_FILES+=usr/include/roken.h
OLD_FILES+=usr/include/roken-common.h
OLD_FILES+=usr/lib/pam_krb5.so.3
OLD_FILES+=usr/lib/pam_krb5.so
OLD_FILES+=usr/lib/pam_ksu.so.3
OLD_FILES+=usr/lib/pam_ksu.so
OLD_FILES+=usr/lib/libasn1.a
OLD_FILES+=usr/lib/libasn1_p.a
OLD_FILES+=usr/lib/libasn1.so.8
OLD_FILES+=usr/lib/libasn1.so
OLD_FILES+=usr/lib/libgssapi_krb5.a
OLD_FILES+=usr/lib/libgssapi_krb5_p.a
OLD_FILES+=usr/lib/libgssapi_krb5.so.8
OLD_FILES+=usr/lib/libgssapi_krb5.so
OLD_FILES+=usr/lib/libhdb.a
OLD_FILES+=usr/lib/libhdb_p.a
OLD_FILES+=usr/lib/libhdb.so.8
OLD_FILES+=usr/lib/libhdb.so
OLD_FILES+=usr/lib/libkadm5clnt.a
OLD_FILES+=usr/lib/libkadm5clnt_p.a
OLD_FILES+=usr/lib/libkadm5clnt.so.8
OLD_FILES+=usr/lib/libkadm5clnt.so
OLD_FILES+=usr/lib/libkadm5srv.a
OLD_FILES+=usr/lib/libkadm5srv_p.a
OLD_FILES+=usr/lib/libkadm5srv.so.8
OLD_FILES+=usr/lib/libkadm5srv.so
OLD_FILES+=usr/lib/libkafs5.a
OLD_FILES+=usr/lib/libkafs5_p.a
OLD_FILES+=usr/lib/libkafs5.so.8
OLD_FILES+=usr/lib/libkafs5.so
OLD_FILES+=usr/lib/libkrb5.a
OLD_FILES+=usr/lib/libkrb5_p.a
OLD_FILES+=usr/lib/libkrb5.so.8
OLD_FILES+=usr/lib/libkrb5.so
OLD_FILES+=usr/lib/libroken.a
OLD_FILES+=usr/lib/libroken_p.a
OLD_FILES+=usr/lib/libroken.so.8
OLD_FILES+=usr/lib/libroken.so
OLD_FILES+=usr/libexec/ipropd-master
OLD_FILES+=usr/libexec/ipropd-slave
OLD_FILES+=usr/libexec/hprop
OLD_FILES+=usr/libexec/hpropd
OLD_FILES+=usr/libexec/kadmind
OLD_FILES+=usr/libexec/kdc
OLD_FILES+=usr/libexec/kpasswdd
OLD_FILES+=usr/sbin/kstash
OLD_FILES+=usr/sbin/ktutil
OLD_FILES+=usr/share/info/heimdal.info.gz
OLD_FILES+=usr/share/man/man1/kdestroy.1.gz
OLD_FILES+=usr/share/man/man1/kinit.1.gz
OLD_FILES+=usr/share/man/man1/klist.1.gz
OLD_FILES+=usr/share/man/man1/kpasswd.1.gz
OLD_FILES+=usr/share/man/man1/krb5-config.1.gz
OLD_FILES+=usr/share/man/man3/kafs5.3.gz
OLD_FILES+=usr/share/man/man3/k_afs_cell_of_file.3.gz
OLD_FILES+=usr/share/man/man3/k_hasafs.3.gz
OLD_FILES+=usr/share/man/man3/k_pioctl.3.gz
OLD_FILES+=usr/share/man/man3/k_setpag.3.gz
OLD_FILES+=usr/share/man/man3/k_unlog.3.gz
OLD_FILES+=usr/share/man/man3/kafs.3.gz
OLD_FILES+=usr/share/man/man3/kafs_set_verbose.3.gz
OLD_FILES+=usr/share/man/man3/kafs_settoken.3.gz
OLD_FILES+=usr/share/man/man3/kafs_settoken5.3.gz
OLD_FILES+=usr/share/man/man3/kafs_settoken_rxkad.3.gz
OLD_FILES+=usr/share/man/man3/krb5_afslog.3.gz
OLD_FILES+=usr/share/man/man3/krb5_afslog_uid.3.gz
OLD_FILES+=usr/share/man/man3/krb_afslog.3.gz
OLD_FILES+=usr/share/man/man3/krb_afslog_uid.3.gz
OLD_FILES+=usr/share/man/man3/krb5.3.gz
OLD_FILES+=usr/share/man/man3/krb5_425_conv_principal.3.gz
OLD_FILES+=usr/share/man/man3/krb5_address.3.gz
OLD_FILES+=usr/share/man/man3/krb5_aname_to_localname.3.gz
OLD_FILES+=usr/share/man/man3/krb5_appdefault.3.gz
OLD_FILES+=usr/share/man/man3/krb5_auth_context.3.gz
OLD_FILES+=usr/share/man/man3/krb5_build_principal.3.gz
OLD_FILES+=usr/share/man/man3/krb5_ccache.3.gz
OLD_FILES+=usr/share/man/man3/krb5_config.3.gz
OLD_FILES+=usr/share/man/man3/krb5_context.3.gz
OLD_FILES+=usr/share/man/man3/krb5_create_checksum.3.gz
OLD_FILES+=usr/share/man/man3/krb5_crypto_init.3.gz
OLD_FILES+=usr/share/man/man3/krb5_data.3.gz
OLD_FILES+=usr/share/man/man3/krb5_encrypt.3.gz
OLD_FILES+=usr/share/man/man3/krb5_free_addresses.3.gz
OLD_FILES+=usr/share/man/man3/krb5_free_principal.3.gz
OLD_FILES+=usr/share/man/man3/krb5_get_all_client_addrs.3.gz
OLD_FILES+=usr/share/man/man3/krb5_get_krbhst.3.gz
OLD_FILES+=usr/share/man/man3/krb5_init_context.3.gz
OLD_FILES+=usr/share/man/man3/krb5_keytab.3.gz
OLD_FILES+=usr/share/man/man3/krb5_krbhst_init.3.gz
OLD_FILES+=usr/share/man/man3/krb5_kuserok.3.gz
OLD_FILES+=usr/share/man/man3/krb5_openlog.3.gz
OLD_FILES+=usr/share/man/man3/krb5_parse_name.3.gz
OLD_FILES+=usr/share/man/man3/krb5_principal_get_realm.3.gz
OLD_FILES+=usr/share/man/man3/krb5_set_default_realm.3.gz
OLD_FILES+=usr/share/man/man3/krb5_sname_to_principal.3.gz
OLD_FILES+=usr/share/man/man3/krb5_timeofday.3.gz
OLD_FILES+=usr/share/man/man3/krb5_unparse_name.3.gz
OLD_FILES+=usr/share/man/man3/krb5_verify_user.3.gz
OLD_FILES+=usr/share/man/man3/krb5_warn.3.gz
OLD_FILES+=usr/share/man/man3/krb5_425_conv_principal_ext.3.gz
OLD_FILES+=usr/share/man/man3/krb5_524_conv_principal.3.gz
OLD_FILES+=usr/share/man/man3/krb5_addr2sockaddr.3.gz
OLD_FILES+=usr/share/man/man3/krb5_address_compare.3.gz
OLD_FILES+=usr/share/man/man3/krb5_address_order.3.gz
OLD_FILES+=usr/share/man/man3/krb5_address_search.3.gz
OLD_FILES+=usr/share/man/man3/krb5_addresses.3.gz
OLD_FILES+=usr/share/man/man3/krb5_anyaddr.3.gz
OLD_FILES+=usr/share/man/man3/krb5_append_addresses.3.gz
OLD_FILES+=usr/share/man/man3/krb5_copy_address.3.gz
OLD_FILES+=usr/share/man/man3/krb5_copy_addresses.3.gz
OLD_FILES+=usr/share/man/man3/krb5_free_address.3.gz
OLD_FILES+=usr/share/man/man3/krb5_h_addr2addr.3.gz
OLD_FILES+=usr/share/man/man3/krb5_h_addr2sockaddr.3.gz
OLD_FILES+=usr/share/man/man3/krb5_make_addrport.3.gz
OLD_FILES+=usr/share/man/man3/krb5_max_sockaddr_size.3.gz
OLD_FILES+=usr/share/man/man3/krb5_parse_address.3.gz
OLD_FILES+=usr/share/man/man3/krb5_print_address.3.gz
OLD_FILES+=usr/share/man/man3/krb5_sockaddr2address.3.gz
OLD_FILES+=usr/share/man/man3/krb5_sockaddr2port.3.gz
OLD_FILES+=usr/share/man/man3/krb5_sockaddr_uninteresting.3.gz
OLD_FILES+=usr/share/man/man3/krb5_appdefault_boolean.3.gz
OLD_FILES+=usr/share/man/man3/krb5_appdefault_string.3.gz
OLD_FILES+=usr/share/man/man3/krb5_appdefault_time.3.gz
OLD_FILES+=usr/share/man/man3/krb5_auth_con_free.3.gz
OLD_FILES+=usr/share/man/man3/krb5_auth_con_genaddrs.3.gz
OLD_FILES+=usr/share/man/man3/krb5_auth_con_getaddrs.3.gz
OLD_FILES+=usr/share/man/man3/krb5_auth_con_getflags.3.gz
OLD_FILES+=usr/share/man/man3/krb5_auth_con_getkey.3.gz
OLD_FILES+=usr/share/man/man3/krb5_auth_con_getlocalsubkey.3.gz
OLD_FILES+=usr/share/man/man3/krb5_auth_con_getrcache.3.gz
OLD_FILES+=usr/share/man/man3/krb5_auth_con_getremotesubkey.3.gz
OLD_FILES+=usr/share/man/man3/krb5_auth_con_getuserkey.3.gz
OLD_FILES+=usr/share/man/man3/krb5_auth_con_init.3.gz
OLD_FILES+=usr/share/man/man3/krb5_log.3.gz
OLD_FILES+=usr/share/man/man3/krb5_auth_con_initivector.3.gz
OLD_FILES+=usr/share/man/man3/krb5_auth_con_setaddrs.3.gz
OLD_FILES+=usr/share/man/man3/krb5_auth_con_setaddrs_from_fd.3.gz
OLD_FILES+=usr/share/man/man3/krb5_auth_con_setflags.3.gz
OLD_FILES+=usr/share/man/man3/krb5_auth_con_setivector.3.gz
OLD_FILES+=usr/share/man/man3/krb5_auth_con_setkey.3.gz
OLD_FILES+=usr/share/man/man3/krb5_auth_con_setlocalsubkey.3.gz
OLD_FILES+=usr/share/man/man3/krb5_auth_con_setrcache.3.gz
OLD_FILES+=usr/share/man/man3/krb5_auth_con_setremotesubkey.3.gz
OLD_FILES+=usr/share/man/man3/krb5_auth_con_setuserkey.3.gz
OLD_FILES+=usr/share/man/man3/krb5_auth_getauthenticator.3.gz
OLD_FILES+=usr/share/man/man3/krb5_auth_getcksumtype.3.gz
OLD_FILES+=usr/share/man/man3/krb5_auth_getkeytype.3.gz
OLD_FILES+=usr/share/man/man3/krb5_auth_getlocalseqnumber.3.gz
OLD_FILES+=usr/share/man/man3/krb5_auth_getremoteseqnumber.3.gz
OLD_FILES+=usr/share/man/man3/krb5_auth_setcksumtype.3.gz
OLD_FILES+=usr/share/man/man3/krb5_auth_setkeytype.3.gz
OLD_FILES+=usr/share/man/man3/krb5_auth_setlocalseqnumber.3.gz
OLD_FILES+=usr/share/man/man3/krb5_auth_setremoteseqnumber.3.gz
OLD_FILES+=usr/share/man/man3/krb5_build_principal_ext.3.gz
OLD_FILES+=usr/share/man/man3/krb5_build_principal_va.3.gz
OLD_FILES+=usr/share/man/man3/krb5_build_principal_va_ext.3.gz
OLD_FILES+=usr/share/man/man3/krb5_make_principal.3.gz
OLD_FILES+=usr/share/man/man3/krb5_cc_close.3.gz
OLD_FILES+=usr/share/man/man3/krb5_cc_copy_cache.3.gz
OLD_FILES+=usr/share/man/man3/krb5_cc_cursor.3.gz
OLD_FILES+=usr/share/man/man3/krb5_cc_default.3.gz
OLD_FILES+=usr/share/man/man3/krb5_cc_default_name.3.gz
OLD_FILES+=usr/share/man/man3/krb5_cc_destroy.3.gz
OLD_FILES+=usr/share/man/man3/krb5_cc_end_seq_get.3.gz
OLD_FILES+=usr/share/man/man3/krb5_cc_gen_new.3.gz
OLD_FILES+=usr/share/man/man3/krb5_cc_get_name.3.gz
OLD_FILES+=usr/share/man/man3/krb5_cc_get_ops.3.gz
OLD_FILES+=usr/share/man/man3/krb5_cc_get_principal.3.gz
OLD_FILES+=usr/share/man/man3/krb5_cc_get_type.3.gz
OLD_FILES+=usr/share/man/man3/krb5_cc_get_version.3.gz
OLD_FILES+=usr/share/man/man3/krb5_cc_initialize.3.gz
OLD_FILES+=usr/share/man/man3/krb5_cc_next_cred.3.gz
OLD_FILES+=usr/share/man/man3/krb5_cc_ops.3.gz
OLD_FILES+=usr/share/man/man3/krb5_cc_register.3.gz
OLD_FILES+=usr/share/man/man3/krb5_cc_remove_cred.3.gz
OLD_FILES+=usr/share/man/man3/krb5_cc_resolve.3.gz
OLD_FILES+=usr/share/man/man3/krb5_cc_retrieve_cred.3.gz
OLD_FILES+=usr/share/man/man3/krb5_cc_set_default_name.3.gz
OLD_FILES+=usr/share/man/man3/krb5_cc_set_flags.3.gz
OLD_FILES+=usr/share/man/man3/krb5_cc_store_cred.3.gz
OLD_FILES+=usr/share/man/man3/krb5_fcc_ops.3.gz
OLD_FILES+=usr/share/man/man3/krb5_mcc_ops.3.gz
OLD_FILES+=usr/share/man/man3/krb5_config_get_bool_default.3.gz
OLD_FILES+=usr/share/man/man3/krb5_config_get_int_default.3.gz
OLD_FILES+=usr/share/man/man3/krb5_config_get_string_default.3.gz
OLD_FILES+=usr/share/man/man3/krb5_config_get_time_default.3.gz
OLD_FILES+=usr/share/man/man3/krb5_checksum_is_collision_proof.3.gz
OLD_FILES+=usr/share/man/man3/krb5_checksum_is_keyed.3.gz
OLD_FILES+=usr/share/man/man3/krb5_vlog.3.gz
OLD_FILES+=usr/share/man/man3/krb5_checksumsize.3.gz
OLD_FILES+=usr/share/man/man3/krb5_verify_checksum.3.gz
OLD_FILES+=usr/share/man/man3/krb5_crypto_destroy.3.gz
OLD_FILES+=usr/share/man/man3/krb5_copy_data.3.gz
OLD_FILES+=usr/share/man/man3/krb5_data_alloc.3.gz
OLD_FILES+=usr/share/man/man3/krb5_data_copy.3.gz
OLD_FILES+=usr/share/man/man3/krb5_data_free.3.gz
OLD_FILES+=usr/share/man/man3/krb5_data_realloc.3.gz
OLD_FILES+=usr/share/man/man3/krb5_data_zero.3.gz
OLD_FILES+=usr/share/man/man3/krb5_free_data.3.gz
OLD_FILES+=usr/share/man/man3/krb5_free_data_contents.3.gz
OLD_FILES+=usr/share/man/man3/krb5_decrypt.3.gz
OLD_FILES+=usr/share/man/man3/krb5_decrypt_EncryptedData.3.gz
OLD_FILES+=usr/share/man/man3/krb5_encrypt_EncryptedData.3.gz
OLD_FILES+=usr/share/man/man3/krb5_get_all_server_addrs.3.gz
OLD_FILES+=usr/share/man/man3/krb5_free_krbhst.3.gz
OLD_FILES+=usr/share/man/man3/krb5_get_krb524hst.3.gz
OLD_FILES+=usr/share/man/man3/krb5_get_krb_admin_hst.3.gz
OLD_FILES+=usr/share/man/man3/krb5_get_krb_changepw_hst.3.gz
OLD_FILES+=usr/share/man/man3/krb5_free_context.3.gz
OLD_FILES+=usr/share/man/man3/krb5_keytab_entry.3.gz
OLD_FILES+=usr/share/man/man3/krb5_kt_add_entry.3.gz
OLD_FILES+=usr/share/man/man3/krb5_kt_close.3.gz
OLD_FILES+=usr/share/man/man3/krb5_kt_compare.3.gz
OLD_FILES+=usr/share/man/man3/krb5_kt_copy_entry_contents.3.gz
OLD_FILES+=usr/share/man/man3/krb5_kt_cursor.3.gz
OLD_FILES+=usr/share/man/man3/krb5_kt_default.3.gz
OLD_FILES+=usr/share/man/man3/krb5_kt_default_name.3.gz
OLD_FILES+=usr/share/man/man3/krb5_kt_end_seq_get.3.gz
OLD_FILES+=usr/share/man/man3/krb5_kt_free_entry.3.gz
OLD_FILES+=usr/share/man/man3/krb5_kt_get_entry.3.gz
OLD_FILES+=usr/share/man/man3/krb5_kt_get_name.3.gz
OLD_FILES+=usr/share/man/man3/krb5_kt_get_type.3.gz
OLD_FILES+=usr/share/man/man3/krb5_kt_next_entry.3.gz
OLD_FILES+=usr/share/man/man3/krb5_kt_ops.3.gz
OLD_FILES+=usr/share/man/man3/krb5_kt_read_service_key.3.gz
OLD_FILES+=usr/share/man/man3/krb5_kt_register.3.gz
OLD_FILES+=usr/share/man/man3/krb5_kt_remove_entry.3.gz
OLD_FILES+=usr/share/man/man3/krb5_kt_resolve.3.gz
OLD_FILES+=usr/share/man/man3/krb5_kt_start_seq_get.3.gz
OLD_FILES+=usr/share/man/man3/krb5_krbhst_format_string.3.gz
OLD_FILES+=usr/share/man/man3/krb5_krbhst_free.3.gz
OLD_FILES+=usr/share/man/man3/krb5_krbhst_get_addrinfo.3.gz
OLD_FILES+=usr/share/man/man3/krb5_krbhst_next.3.gz
OLD_FILES+=usr/share/man/man3/krb5_krbhst_reset.3.gz
OLD_FILES+=usr/share/man/man3/krb5_krbhst_next_as_string.3.gz
OLD_FILES+=usr/share/man/man3/krb5_addlog_dest.3.gz
OLD_FILES+=usr/share/man/man3/krb5_addlog_func.3.gz
OLD_FILES+=usr/share/man/man3/krb5_closelog.3.gz
OLD_FILES+=usr/share/man/man3/krb5_initlog.3.gz
OLD_FILES+=usr/share/man/man3/krb5_log_msg.3.gz
OLD_FILES+=usr/share/man/man3/krb5_vlog_msg.3.gz
OLD_FILES+=usr/share/man/man3/krb5_principal_get_comp_string.3.gz
OLD_FILES+=usr/share/man/man3/krb5_free_host_realm.3.gz
OLD_FILES+=usr/share/man/man3/krb5_get_default_realm.3.gz
OLD_FILES+=usr/share/man/man3/krb5_get_default_realms.3.gz
OLD_FILES+=usr/share/man/man3/krb5_get_host_realm.3.gz
OLD_FILES+=usr/share/man/man3/krb5_sock_to_principal.3.gz
OLD_FILES+=usr/share/man/man3/krb5_us_timeofday.3.gz
OLD_FILES+=usr/share/man/man3/krb5_verify_opt_init.3.gz
OLD_FILES+=usr/share/man/man3/krb5_verify_opt_set_flags.3.gz
OLD_FILES+=usr/share/man/man3/krb5_verify_opt_set_keytab.3.gz
OLD_FILES+=usr/share/man/man3/krb5_verify_opt_set_secure.3.gz
OLD_FILES+=usr/share/man/man3/krb5_verify_opt_set_service.3.gz
OLD_FILES+=usr/share/man/man3/krb5_verify_user_lrealm.3.gz
OLD_FILES+=usr/share/man/man3/krb5_verify_user_opt.3.gz
OLD_FILES+=usr/share/man/man3/krb5_err.3.gz
OLD_FILES+=usr/share/man/man3/krb5_errx.3.gz
OLD_FILES+=usr/share/man/man3/krb5_set_warn_dest.3.gz
OLD_FILES+=usr/share/man/man3/krb5_verr.3.gz
OLD_FILES+=usr/share/man/man3/krb5_verrx.3.gz
OLD_FILES+=usr/share/man/man3/krb5_vwarn.3.gz
OLD_FILES+=usr/share/man/man3/krb5_vwarnx.3.gz
OLD_FILES+=usr/share/man/man3/krb5_warnx.3.gz
OLD_FILES+=usr/share/man/man5/krb5.conf.5.gz
OLD_FILES+=usr/share/man/man8/pam_krb5.8.gz
OLD_FILES+=usr/share/man/man8/pam_ksu.8.gz
OLD_FILES+=usr/share/man/man8/kerberos.8.gz
OLD_FILES+=usr/share/man/man8/hprop.8.gz
OLD_FILES+=usr/share/man/man8/hpropd.8.gz
OLD_FILES+=usr/share/man/man8/kadmind.8.gz
OLD_FILES+=usr/share/man/man8/kdc.8.gz
OLD_FILES+=usr/share/man/man8/kpasswdd.8.gz
OLD_FILES+=usr/share/man/man8/kadmin.8.gz
OLD_FILES+=usr/share/man/man8/verify_krb5_conf.8.gz
OLD_FILES+=usr/share/man/man8/kstash.8.gz
OLD_FILES+=usr/share/man/man8/ktutil.8.gz
.endif

#.if ${MK_LIB32} == no
# to be filled in
#.endif

#.if ${MK_LIBPTHREAD} == no
# to be filled in
#.endif

#.if ${MK_THR} == no
# to be filled in
#.endif

#.if ${MK_LOCALES} == no
# to be filled in
#.endif

.if ${MK_LPR} == no
OLD_FILES+=usr/bin/lp
OLD_FILES+=usr/bin/lpq
OLD_FILES+=usr/bin/lpr
OLD_FILES+=usr/bin/lprm
OLD_FILES+=usr/libexec/lpr/ru/bjc-240.sh.sample
OLD_FILES+=usr/libexec/lpr/ru/koi2alt
OLD_FILES+=usr/libexec/lpr/ru/koi2855
OLD_FILES+=usr/libexec/lpr/lpf
OLD_FILES+=usr/sbin/chkprintcap
OLD_FILES+=usr/sbin/lpc
OLD_FILES+=usr/sbin/lpd
OLD_FILES+=usr/sbin/lptest
OLD_FILES+=usr/sbin/pac
OLD_FILES+=usr/share/doc/smm/07.lpd/paper.ascii.gz
OLD_FILES+=usr/share/man/man1/lp.1.gz
OLD_FILES+=usr/share/man/man1/lpq.1.gz
OLD_FILES+=usr/share/man/man1/lpr.1.gz
OLD_FILES+=usr/share/man/man1/lprm.1.gz
OLD_FILES+=usr/share/man/man1/lptest.1.gz
OLD_FILES+=usr/share/man/man5/printcap.5.gz
OLD_FILES+=usr/share/man/man8/chkprintcap.8.gz
OLD_FILES+=usr/share/man/man8/lpc.8.gz
OLD_FILES+=usr/share/man/man8/lpd.8.gz
OLD_FILES+=usr/share/man/man8/pac.8.gz
.endif

#.if ${MK_MAILWRAPPER} == no
# to be filled in
#.endif

#.if ${MK_MAN} == no
# This should add a dependency to a special target which removes all man pages.
# Listing all of them here is overkill.
#.endif

#.if ${MK_NCP} == no
# to be filled in
#.endif

.if ${MK_NETCAT} == no
OLD_FILES+=usr/bin/nc
OLD_FILES+=usr/share/man/man1/nc.1.gz
.endif

#.if ${MK_NIS} == no
# to be filled in
#.endif

#.if ${MK_NLS} == no
# to be filled in
#.endif

#.if ${MK_OBJC} == no
# to be filled in
#.endif

#.if ${MK_OPENSSH} == no
# to be filled in
#.endif

#.if ${MK_OPENSSL} == no
# to be filled in
#.endif

#.if ${MK_PCVT} == no
# to be filled in
#.endif

.if ${MK_PROFILE} == no
OLD_FILES+=usr/lib/libalias_cuseeme_p.a
OLD_FILES+=usr/lib/libalias_dummy_p.a
OLD_FILES+=usr/lib/libalias_ftp_p.a
OLD_FILES+=usr/lib/libalias_irc_p.a
OLD_FILES+=usr/lib/libalias_nbt_p.a
OLD_FILES+=usr/lib/libalias_p.a
OLD_FILES+=usr/lib/libalias_pptp_p.a
OLD_FILES+=usr/lib/libalias_skinny_p.a
OLD_FILES+=usr/lib/libalias_smedia_p.a
OLD_FILES+=usr/lib/libarchive_p.a
OLD_FILES+=usr/lib/libasn1_p.a
OLD_FILES+=usr/lib/libatm_p.a
OLD_FILES+=usr/lib/libbegemot_p.a
OLD_FILES+=usr/lib/libbluetooth_p.a
OLD_FILES+=usr/lib/libbsdxml_p.a
OLD_FILES+=usr/lib/libbsm_p.a
OLD_FILES+=usr/lib/libbsnmp_p.a
OLD_FILES+=usr/lib/libbz2_p.a
OLD_FILES+=usr/lib/libc_p.a
OLD_FILES+=usr/lib/libcalendar_p.a
OLD_FILES+=usr/lib/libcam_p.a
OLD_FILES+=usr/lib/libcom_err_p.a
OLD_FILES+=usr/lib/libcompat_p.a
OLD_FILES+=usr/lib/libcrypt_p.a
OLD_FILES+=usr/lib/libcrypto_p.a
OLD_FILES+=usr/lib/libcurses_p.a
OLD_FILES+=usr/lib/libcursesw_p.a
OLD_FILES+=usr/lib/libdevinfo_p.a
OLD_FILES+=usr/lib/libdevstat_p.a
OLD_FILES+=usr/lib/libdialog_p.a
OLD_FILES+=usr/lib/libedit_p.a
OLD_FILES+=usr/lib/libelf_p.a
OLD_FILES+=usr/lib/libfetch_p.a
OLD_FILES+=usr/lib/libfl_p.a
OLD_FILES+=usr/lib/libform_p.a
OLD_FILES+=usr/lib/libformw_p.a
OLD_FILES+=usr/lib/libftpio_p.a
OLD_FILES+=usr/lib/libg2c_p.a
OLD_FILES+=usr/lib/libgcc_p.a
OLD_FILES+=usr/lib/libgeom_p.a
OLD_FILES+=usr/lib/libgnuregex_p.a
OLD_FILES+=usr/lib/libgpib_p.a
OLD_FILES+=usr/lib/libgssapi_krb5_p.a
OLD_FILES+=usr/lib/libgssapi_p.a
OLD_FILES+=usr/lib/libhdb_p.a
OLD_FILES+=usr/lib/libhistory_p.a
OLD_FILES+=usr/lib/libipsec_p.a
OLD_FILES+=usr/lib/libipx_p.a
OLD_FILES+=usr/lib/libkadm5clnt_p.a
OLD_FILES+=usr/lib/libkadm5srv_p.a
OLD_FILES+=usr/lib/libkafs5_p.a
OLD_FILES+=usr/lib/libkeycap_p.a
OLD_FILES+=usr/lib/libkiconv_p.a
OLD_FILES+=usr/lib/libkrb5_p.a
OLD_FILES+=usr/lib/libkvm_p.a
OLD_FILES+=usr/lib/libl_p.a
OLD_FILES+=usr/lib/libln_p.a
OLD_FILES+=usr/lib/liblwres_p.a
OLD_FILES+=usr/lib/libm_p.a
OLD_FILES+=usr/lib/libmagic_p.a
OLD_FILES+=usr/lib/libmd_p.a
OLD_FILES+=usr/lib/libmemstat_p.a
OLD_FILES+=usr/lib/libmenu_p.a
OLD_FILES+=usr/lib/libmenuw_p.a
OLD_FILES+=usr/lib/libmilter_p.a
OLD_FILES+=usr/lib/libmp_p.a
OLD_FILES+=usr/lib/libmytinfo_p.a
OLD_FILES+=usr/lib/libncp_p.a
OLD_FILES+=usr/lib/libncurses_p.a
OLD_FILES+=usr/lib/libncursesw_p.a
OLD_FILES+=usr/lib/libnetgraph_p.a
OLD_FILES+=usr/lib/libngatm_p.a
OLD_FILES+=usr/lib/libobjc_p.a
OLD_FILES+=usr/lib/libopie_p.a
OLD_FILES+=usr/lib/libpanel_p.a
OLD_FILES+=usr/lib/libpanelw_p.a
OLD_FILES+=usr/lib/libpcap_p.a
OLD_FILES+=usr/lib/libpmc_p.a
OLD_FILES+=usr/lib/libpthread_p.a
OLD_FILES+=usr/lib/libradius_p.a
OLD_FILES+=usr/lib/libreadline_p.a
OLD_FILES+=usr/lib/libroken_p.a
OLD_FILES+=usr/lib/librpcsvc_p.a
OLD_FILES+=usr/lib/librt_p.a
OLD_FILES+=usr/lib/libsbuf_p.a
OLD_FILES+=usr/lib/libsdp_p.a
OLD_FILES+=usr/lib/libsmb_p.a
OLD_FILES+=usr/lib/libssh_p.a
OLD_FILES+=usr/lib/libssl_p.a
OLD_FILES+=usr/lib/libstdc++_p.a
OLD_FILES+=usr/lib/libsupc++_p.a
OLD_FILES+=usr/lib/libtacplus_p.a
OLD_FILES+=usr/lib/libtermcap_p.a
OLD_FILES+=usr/lib/libtermcapw_p.a
OLD_FILES+=usr/lib/libtermlib_p.a
OLD_FILES+=usr/lib/libtermlibw_p.a
OLD_FILES+=usr/lib/libthr_p.a
OLD_FILES+=usr/lib/libthread_db_p.a
OLD_FILES+=usr/lib/libtinfo_p.a
OLD_FILES+=usr/lib/libtinfow_p.a
OLD_FILES+=usr/lib/libufs_p.a
OLD_FILES+=usr/lib/libugidfw_p.a
OLD_FILES+=usr/lib/libusbhid_p.a
OLD_FILES+=usr/lib/libutil_p.a
OLD_FILES+=usr/lib/libvgl_p.a
OLD_FILES+=usr/lib/libwrap_p.a
OLD_FILES+=usr/lib/liby_p.a
OLD_FILES+=usr/lib/libypclnt_p.a
OLD_FILES+=usr/lib/libz_p.a
.endif

.if ${MK_RCMDS} == no
OLD_FILES+=bin/rcp
OLD_FILES+=rescue/rcp
OLD_FILES+=usr/bin/rlogin
OLD_FILES+=usr/bin/rsh
OLD_FILES+=usr/libexec/rlogind
OLD_FILES+=usr/libexec/rshd
OLD_FILES+=usr/share/man/man1/rcp.1.gz
OLD_FILES+=usr/share/man/man1/rlogin.1.gz
OLD_FILES+=usr/share/man/man1/rsh.1.gz
OLD_FILES+=usr/share/man/man8/rlogind.8.gz
OLD_FILES+=usr/share/man/man8/rshd.8.gz
.endif

#.if ${MK_RCS} == no
# to be filled in
#.endif

#.if ${MK_RESCUE} == no
# to be filled in or replaced with a special target
#.endif

#.if ${MK_SENDMAIL} == no
# to be filled in
#.endif

#.if ${MK_SHAREDOCS} == no
# to be filled in
#.endif

#.if ${MK_SYSCONS} == no
# to be filled in
#.endif

.if ${MK_TCSH} == no
OLD_FILES+=bin/csh
OLD_FILES+=bin/tcsh
OLD_FILES+=rescue/csh
OLD_FILES+=rescue/tcsh
OLD_FILES+=usr/share/examples/tcsh/complete.tcsh
OLD_FILES+=usr/share/examples/tcsh/csh-mode.el
OLD_FILES+=usr/share/man/man1/csh.1.gz
OLD_FILES+=usr/share/man/man1/tcsh.1.gz
OLD_FILES+=usr/share/nls/de_AT.ISO8859-1/tcsh.cat
OLD_FILES+=usr/share/nls/de_AT.ISO8859-15/tcsh.cat
OLD_FILES+=usr/share/nls/de_CH.ISO8859-1/tcsh.cat
OLD_FILES+=usr/share/nls/de_CH.ISO8859-15/tcsh.cat
OLD_FILES+=usr/share/nls/de_DE.ISO8859-1/tcsh.cat
OLD_FILES+=usr/share/nls/de_DE.ISO8859-15/tcsh.cat
OLD_FILES+=usr/share/nls/el_GR.ISO8859-7/tcsh.cat
OLD_FILES+=usr/share/nls/es_ES.ISO8859-1/tcsh.cat
OLD_FILES+=usr/share/nls/es_ES.ISO8859-15/tcsh.cat
OLD_FILES+=usr/share/nls/et_EE.ISO8859-15/tcsh.cat
OLD_FILES+=usr/share/nls/fi_FI.ISO8859-1/tcsh.cat
OLD_FILES+=usr/share/nls/fi_FI.ISO8859-15/tcsh.cat
OLD_FILES+=usr/share/nls/fr_BE.ISO8859-1/tcsh.cat
OLD_FILES+=usr/share/nls/fr_BE.ISO8859-15/tcsh.cat
OLD_FILES+=usr/share/nls/fr_CA.ISO8859-1/tcsh.cat
OLD_FILES+=usr/share/nls/fr_CA.ISO8859-15/tcsh.cat
OLD_FILES+=usr/share/nls/fr_CH.ISO8859-1/tcsh.cat
OLD_FILES+=usr/share/nls/fr_CH.ISO8859-15/tcsh.cat
OLD_FILES+=usr/share/nls/fr_FR.ISO8859-1/tcsh.cat
OLD_FILES+=usr/share/nls/fr_FR.ISO8859-15/tcsh.cat
OLD_FILES+=usr/share/nls/it_CH.ISO8859-1/tcsh.cat
OLD_FILES+=usr/share/nls/it_CH.ISO8859-15/tcsh.cat
OLD_FILES+=usr/share/nls/it_IT.ISO8859-1/tcsh.cat
OLD_FILES+=usr/share/nls/it_IT.ISO8859-15/tcsh.cat
OLD_FILES+=usr/share/nls/ja_JP.eucJP/tcsh.cat
OLD_FILES+=usr/share/nls/ru_RU.KOI8-R/tcsh.cat
OLD_FILES+=usr/share/nls/uk_UA.KOI8-U/tcsh.cat
.endif

#.if ${MK_TOOLCHAIN} == no
# to be filled in
#.endif

#.if ${MK_USB} == no
# to be filled in
#.endif
