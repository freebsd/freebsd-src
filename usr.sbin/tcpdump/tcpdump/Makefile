#	$Id: Makefile,v 1.16 1997/02/22 16:14:02 peter Exp $

PROG=	tcpdump
CFLAGS+=-DHAVE_FCNTL_H=1 -DHAVE_NET_SLIP_H=1 -DTIME_WITH_SYS_TIME=1 \
	-DHAVE_ETHER_NTOA=1 -DHAVE_SETLINEBUF=1 -DSTDC_HEADERS=1 \
	-DRETSIGTYPE=void -DRETSIGVAL= -DHAVE_SOCKADDR_SA_LEN=1 \
	-DHAVE_TM_GMTOFF=1 -DLBL_ALIGN=1 -DPPP -DHAVE_FDDI
MAN1=	tcpdump.1
SRCS =	version.c tcpdump.c \
	print-arp.c print-atalk.c print-atm.c print-bootp.c \
	print-decnet.c print-domain.c print-dvmrp.c print-egp.c \
	print-ether.c print-fddi.c print-gre.c print-icmp.c \
	print-igrp.c print-ip.c print-ipx.c print-isoclns.c print-krb.c \
	print-llc.c print-nfs.c print-ntp.c print-null.c print-ospf.c \
	print-pim.c print-ppp.c print-rip.c print-sl.c print-snmp.c \
	print-sunrpc.c print-tcp.c print-tftp.c print-udp.c print-wb.c \
	addrtoname.c bpf_dump.c machdep.c parsenfsfh.c util.c
CLEANFILES+=	version.c
DPADD+=	${LIBL} ${LIBPCAP}
LDADD+=	-ll -lpcap

TCPDUMP_DISTDIR?= ${.CURDIR}/../../../contrib/tcpdump
CFLAGS+= -I${TCPDUMP_DISTDIR}/lbl
.PATH:	${TCPDUMP_DISTDIR}

version.c: ${TCPDUMP_DISTDIR}/VERSION
	rm -f version.c ; \
	sed 's/.*/char version[] = "&";/' ${TCPDUMP_DISTDIR}/VERSION > version.c

.include <bsd.prog.mk>
