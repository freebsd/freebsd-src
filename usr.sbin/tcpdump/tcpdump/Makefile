#	$Id: Makefile,v 1.10 1995/08/29 19:48:10 wollman Exp $

PROG=	tcpdump
CFLAGS+=-DCSLIP -DPPP -DFDDI -DETHER_SERVICE
MAN1=	tcpdump.1
SRCS=	version.c addrtoname.c bpf_dump.c parsenfsfh.c \
	print-arp.c print-atalk.c print-bootp.c print-decnet.c print-domain.c \
	print-egp.c print-ether.c print-fddi.c print-icmp.c print-ip.c \
	print-ipx.c print-isoclns.c print-llc.c print-krb.c \
	print-nfs.c print-ntp.c print-null.c print-ospf.c print-ppp.c \
	print-rip.c print-sl.c print-snmp.c print-sunrpc.c print-tcp.c \
	print-tftp.c print-udp.c print-wb.c tcpdump.c \
	util.c
CLEANFILES+=	version.c
DPADD+=	${LIBL} ${LIBPCAP}
LDADD+=	-ll -lpcap

version.c: VERSION
	rm -f version.c ; \
	sed 's/.*/char version[] = "&";/' $(.CURDIR)/VERSION > version.c

.include <bsd.prog.mk>
