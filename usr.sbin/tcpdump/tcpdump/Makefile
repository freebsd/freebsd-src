# $FreeBSD$

PROG=	tcpdump
CFLAGS+=-DHAVE_CONFIG_H -I${.CURDIR}
SRCS =	version.c tcpdump.c \
	addrtoname.c bpf_dump.c gmt2local.c machdep.c parsenfsfh.c \
	print-ah.c print-arp.c print-ascii.c print-atalk.c print-atm.c \
	print-bgp.c print-bootp.c print-chdlc.c print-cip.c print-decnet.c \
	print-dhcp6.c print-domain.c print-dvmrp.c print-egp.c print-esp.c \
	print-ether.c print-fddi.c print-frag6.c print-gre.c print-icmp.c \
	print-icmp6.c print-igrp.c print-ip.c print-ip6.c print-ip6opts.c \
	print-ipcomp.c print-ipx.c print-isakmp.c print-isoclns.c print-krb.c \
	print-l2tp.c print-lane.c print-lcp.c print-llc.c print-mobile.c \
	print-nfs.c print-ntp.c print-null.c print-ospf.c print-ospf6.c \
	print-pim.c print-ppp.c print-pppoe.c print-raw.c print-rip.c \
	print-ripng.c print-rt6.c print-rx.c print-sl.c print-smb.c \
	print-snmp.c print-sunrpc.c print-tcp.c print-telnet.c print-tftp.c \
	print-token.c print-udp.c print-vjc.c print-wb.c \
	savestr.c setsignal.c smbutil.c util.c
CLEANFILES+=	version.c
DPADD+=	${LIBL} ${LIBPCAP}
LDADD+=	-ll -lpcap
.if exists(../../../secure) && !defined(NOCRYPT) && !defined(NOSECURE) && \
	!defined(NO_OPENSSL) && !defined(RELEASE_CRUNCH)
DISTRIBUTION=crypto
DPADD+= ${LIBCRYPTO}
LDADD+= -lcrypto
CFLAGS+= -I${DESTDIR}/usr/include/openssl -DCRYPTO -DHAVE_LIBCRYPTO -DHAVE_RC5_H -DHAVE_CAST_H
.endif

TCPDUMP_DISTDIR?= ${.CURDIR}/../../../contrib/tcpdump
CFLAGS+= -I${TCPDUMP_DISTDIR}/lbl
.PATH:	${TCPDUMP_DISTDIR}

version.c: ${TCPDUMP_DISTDIR}/VERSION
	rm -f version.c ; \
	sed 's/.*/char version[] = "&";/' ${TCPDUMP_DISTDIR}/VERSION > version.c

.include <bsd.prog.mk>
