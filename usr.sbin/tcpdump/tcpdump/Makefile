# $FreeBSD$

PROG=	tcpdump
CFLAGS+=-DHAVE_CONFIG_H -I${.CURDIR}
SRCS =	version.c tcpdump.c \
	print-arp.c print-atalk.c print-atm.c print-bootp.c \
	print-decnet.c print-domain.c print-dvmrp.c print-egp.c \
	print-ether.c print-fddi.c print-gre.c print-icmp.c print-igmp.c \
	print-igrp.c print-ip.c print-ipx.c print-isoclns.c print-krb.c \
	print-llc.c print-nfs.c print-ntp.c print-null.c print-ospf.c \
	print-pim.c print-ppp.c print-raw.c print-rip.c print-sl.c \
	print-snmp.c print-stp.c print-sunrpc.c print-tcp.c print-tftp.c \
	print-udp.c print-wb.c addrtoname.c gmt2local.c machdep.c \
	parsenfsfh.c util.c savestr.c setsignal.c \
	print-esp.c print-ah.c print-vjc.c print-isakmp.c print-chdlc.c \
	print-ipcomp.c print-mobile.c print-l2tp.c print-bgp.c print-rx.c \
	print-lane.c print-cip.c print-pppoe.c print-lcp.c \
	print-smb.c smbutil.c print-ascii.c print-telnet.c print-cnfp.c \
	print-vrrp.c print-cdp.c print-token.c print-bxxp.c print-timed.c \
	print-radius.c print-sll.c \
	print-ip6.c print-ip6opts.c print-ripng.c print-icmp6.c \
	print-frag6.c print-rt6.c print-ospf6.c print-dhcp6.c 
CLEANFILES+=	version.c
DPADD+=	${LIBL} ${LIBPCAP}
LDADD+=	-ll -lpcap
.if !defined(NOINET6)
CFLAGS+=	-DINET6
.endif
.if exists(../../../secure) && !defined(NOCRYPT) && !defined(NOSECURE) && \
	!defined(NO_OPENSSL) && !defined(RELEASE_CRUNCH)
DISTRIBUTION=crypto
DPADD+= ${LIBCRYPTO}
LDADD+= -lcrypto
CFLAGS+= -I${DESTDIR}/usr/include/openssl -DHAVE_LIBCRYPTO -DHAVE_RC5_H -DHAVE_CAST_H
.endif

TCPDUMP_DISTDIR?= ${.CURDIR}/../../../contrib/tcpdump
CFLAGS+= -I${TCPDUMP_DISTDIR}/lbl
.PATH:	${TCPDUMP_DISTDIR}

version.c: ${TCPDUMP_DISTDIR}/VERSION
	rm -f version.c ; \
	sed 's/.*/char version[] = "&";/' ${TCPDUMP_DISTDIR}/VERSION > version.c

.include <bsd.prog.mk>
