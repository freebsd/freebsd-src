#	@(#)Makefile	0.1 (RWGrimes) 3/24/93

PROG=	tcpdump
CFLAGS+=-DCSLIP -DPPP -I.
MAN1=	tcpdump.1
SRCS=	version.c addrtoname.c bpf_dump.c bpf_filter.c bpf_image.c etherent.c \
	gencode.c inet.c md.c nametoaddr.c optimize.c os.c pcap.c \
	print-arp.c print-atalk.c print-bootp.c print-domain.c \
	print-egp.c print-ether.c print-fddi.c print-icmp.c print-ip.c \
	print-nfs.c print-ntp.c print-null.c print-ospf.c print-ppp.c \
	print-rip.c print-sl.c print-snmp.c print-sunrpc.c print-tcp.c \
	print-tftp.c print-udp.c savefile.c tcpdump.c tcpgram.c \
	tcplex.c util.c
.PATH:	/sys/net
CLEANFILES+=	tcpgram.c tcplex.c y.tab.h y.tab.c version.c version.h

version.c version.h: VERSION
	rm -f version.c ; \
	sed 's/.*/char version[] = "&";/' $(.CURDIR)/VERSION > version.c
	set `sed 's/\([0-9]*\)\.\([0-9]*\).*/\1 \2/' $(.CURDIR)/VERSION` ; \
		{ echo '#define VERSION_MAJOR' $$1 ; \
		  echo '#define VERSION_MINOR' $$2 ; } > version.h

.include <bsd.prog.mk>
