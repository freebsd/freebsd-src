#	$Id: Makefile,v 1.14 1996/08/21 16:50:15 pst Exp $

PROG=	tcpdump
CFLAGS+=-DHAVE_FCNTL_H=1 -DHAVE_NET_SLIP_H=1 -DTIME_WITH_SYS_TIME=1 \
	-DHAVE_ETHER_NTOA=1 -DHAVE_SETLINEBUF=1 -DSTDC_HEADERS=1 \
	-DRETSIGTYPE=void -DRETSIGVAL= -DHAVE_SOCKADDR_SA_LEN=1 \
	-DHAVE_TM_GMTOFF=1 -DLBL_ALIGN=1 -DPPP -DHAVE_FDDI
MAN1=	tcpdump.1
SRCS=	version.c tcpdump.c addrtoname.c \
        print-ether.c print-ip.c print-arp.c print-tcp.c \
	print-udp.c print-atalk.c print-domain.c print-tftp.c \
	print-bootp.c print-nfs.c print-icmp.c print-igrp.c \
	print-sl.c print-ppp.c print-rip.c print-snmp.c \
	print-ntp.c print-null.c print-egp.c print-ospf.c \
	print-fddi.c print-llc.c print-sunrpc.c print-wb.c \
	print-decnet.c print-isoclns.c print-ipx.c \
	print-atm.c print-fr.c print-dvmrp.c print-pim.c print-krb.c \
	util.c machdep.c bpf_dump.c parsenfsfh.c
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
