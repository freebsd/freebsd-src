# $FreeBSD$

TCPDUMP_DISTDIR?= ${.CURDIR}/../../../contrib/tcpdump
.PATH: ${TCPDUMP_DISTDIR}

PROG=	tcpdump
SRCS=	addrtoname.c gmt2local.c machdep.c parsenfsfh.c \
	print-802_11.c print-ah.c print-arcnet.c print-arp.c \
	print-ascii.c print-atalk.c print-atm.c print-bgp.c \
	print-bootp.c print-beep.c print-cdp.c print-chdlc.c \
	print-cip.c print-cnfp.c print-decnet.c print-domain.c \
	print-dvmrp.c print-egp.c print-esp.c print-ether.c \
	print-fddi.c print-gre.c print-hsrp.c print-icmp.c \
	print-igmp.c print-igrp.c print-ip.c print-ipcomp.c \
	print-ipx.c print-isakmp.c print-isoclns.c print-krb.c \
	print-l2tp.c print-lane.c print-lcp.c print-llc.c print-lwres.c \
	print-msdp.c print-mobile.c print-mpls.c print-nfs.c \
	print-ntp.c print-null.c print-ospf.c print-pim.c \
	print-ppp.c print-pppoe.c print-pptp.c print-radius.c \
	print-raw.c print-rip.c print-rx.c print-sctp.c \
	print-sl.c print-sll.c print-smb.c print-snmp.c \
	print-stp.c print-sunrpc.c print-tcp.c \
	print-telnet.c print-tftp.c print-timed.c print-token.c \
	print-udp.c print-vjc.c print-vrrp.c print-wb.c print-zephyr.c \
	setsignal.c smbutil.c tcpdump.c util.c version.c
CLEANFILES+=	version.c

CFLAGS+= -I${.CURDIR} -I${TCPDUMP_DISTDIR}
CFLAGS+= -DHAVE_CONFIG_H

.if !defined(NOINET6)
SRCS+=	print-ip6.c print-ip6opts.c print-ripng.c print-icmp6.c \
	print-frag6.c print-rt6.c print-ospf6.c print-dhcp6.c
CFLAGS+=	-DINET6
.endif
.if ${MACHINE_ARCH} != "i386"
CFLAGS+=	-DLBL_ALIGN
.endif

DPADD=	${LIBL} ${LIBPCAP}
LDADD=	-ll -lpcap
.if exists(../../../crypto) && !defined(NOCRYPT) && \
	!defined(NO_OPENSSL) && !defined(RELEASE_CRUNCH)
DISTRIBUTION=crypto
DPADD+= ${LIBCRYPTO}
LDADD+= -lcrypto
CFLAGS+= -I${DESTDIR}/usr/include/openssl -DHAVE_LIBCRYPTO -DHAVE_RC5_H -DHAVE_CAST_H
.endif

version.c: ${TCPDUMP_DISTDIR}/VERSION
	rm -f version.c ; \
	    sed 's/.*/char version[] = "&";/' ${TCPDUMP_DISTDIR}/VERSION \
		> version.c

.include <bsd.prog.mk>
