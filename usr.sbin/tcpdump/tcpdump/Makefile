# $FreeBSD$

.include <bsd.own.mk>

TCPDUMP_DISTDIR?= ${.CURDIR}/../../../contrib/tcpdump
.PATH: ${TCPDUMP_DISTDIR}

PROG=	tcpdump

SRCS =	addrtoname.c af.c checksum.c cpack.c gmpls.c oui.c gmt2local.c \
        ipproto.c nlpid.c l2vpn.c machdep.c parsenfsfh.c \
	print-802_11.c print-ap1394.c print-ah.c print-arcnet.c \
	print-aodv.c print-arp.c print-ascii.c print-atalk.c print-atm.c \
	print-beep.c print-bfd.c print-bgp.c print-bootp.c print-cdp.c \
	print-cfm.c print-chdlc.c print-cip.c print-cnfp.c print-dccp.c \
	print-decnet.c print-domain.c print-dtp.c print-dvmrp.c print-enc.c \
	print-egp.c print-eap.c print-eigrp.c \
	print-esp.c print-ether.c print-fddi.c print-forces.c print-fr.c \
	print-gre.c print-hsrp.c print-icmp.c print-igmp.c \
	print-igrp.c print-ip.c print-ipcomp.c print-ipfc.c print-ipnet.c \
	print-ipx.c print-isakmp.c print-isoclns.c print-juniper.c print-krb.c \
	print-l2tp.c print-lane.c print-ldp.c print-lldp.c print-llc.c \
        print-lmp.c print-lspping.c \
	print-lwapp.c print-lwres.c print-mobile.c print-mpls.c print-msdp.c \
	print-mpcp.c \
	print-nfs.c print-ntp.c print-null.c print-olsr.c print-ospf.c \
	print-pgm.c print-pim.c print-ppp.c print-pppoe.c \
	print-pptp.c print-radius.c print-raw.c print-rip.c \
	print-rrcp.c print-rsvp.c print-rx.c print-sctp.c print-sflow.c \
	print-sip.c print-sl.c print-sll.c \
	print-slow.c print-snmp.c print-stp.c print-sunatm.c print-sunrpc.c \
	print-symantec.c print-syslog.c print-tcp.c print-telnet.c print-tftp.c \
	print-timed.c print-token.c print-udld.c print-udp.c print-vjc.c \
	print-vqp.c print-vrrp.c print-vtp.c \
	print-wb.c print-zephyr.c setsignal.c tcpdump.c util.c \
	print-smb.c signature.c smbutil.c \
	version.c
CLEANFILES+=	version.c

CFLAGS+= -I${.CURDIR} -I${TCPDUMP_DISTDIR}
CFLAGS+= -DHAVE_CONFIG_H
CFLAGS+= -D_U_="__attribute__((unused))"

.if ${MK_INET6_SUPPORT} != "no"
SRCS+=	print-ip6.c print-ip6opts.c print-mobility.c print-ripng.c \
	print-icmp6.c print-frag6.c print-rt6.c print-ospf6.c print-dhcp6.c
CFLAGS+=	-DINET6
.endif
.if ${MACHINE_CPUARCH} != "i386"
CFLAGS+=	-DLBL_ALIGN
.endif

DPADD=	${LIBL} ${LIBPCAP}
LDADD=	-ll -lpcap
.if ${MK_OPENSSL} != "no" && !defined(RELEASE_CRUNCH)
DPADD+= ${LIBCRYPTO}
LDADD+= -lcrypto
CFLAGS+= -I${DESTDIR}/usr/include/openssl
CFLAGS+= -DHAVE_LIBCRYPTO -DHAVE_OPENSSL_EVP_H
.endif

.if ${MK_PF} != "no"
SRCS+=	print-pflog.c
CFLAGS+= -DHAVE_NET_PFVAR_H
.endif

version.c: ${TCPDUMP_DISTDIR}/VERSION
	rm -f version.c ; \
	    sed 's/.*/char version[] = "&";/' ${TCPDUMP_DISTDIR}/VERSION \
		> version.c

.include <bsd.prog.mk>
