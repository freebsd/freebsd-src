# Makefile for libpcap
# $Id: Makefile,v 1.9 1996/08/19 21:00:45 pst Exp $

LIB=    pcap
SRCS=	pcap-bpf.c \
        pcap.c inet.c gencode.c optimize.c nametoaddr.c \
	etherent.c savefile.c bpf_filter.c bpf_image.c \
        grammar.y scanner.l
MAN3=	pcap.3
CLEANFILES+= lex.yy.c tokdefs.h grammar.c scanner.c

DEFS=	-DHAVE_SYS_IOCCOM_H=1 -DHAVE_SYS_SOCKIO_H=1 \
	-DHAVE_ETHER_HOSTTON=1 -DHAVE_STRERROR=1 \
	-DHAVE_SOCKADDR_SA_LEN=1 -DLBL_ALIGN=1 

CFLAGS+=-Wall -I. -I${.CURDIR} -Dyylval=pcap_lval -Dlint ${DEFS}

SHLIB_MAJOR=2
SHLIB_MINOR=1

#
# Magic to grab sources out of src/contrib
#
DISTDIR?=${.CURDIR}/../../contrib/libpcap
CFLAGS+=-I${DISTDIR} -I${DISTDIR}/lbl
.PATH:	${DISTDIR}
.PATH:	${DISTDIR}/bpf/net

beforeinstall:
	-cmp -s ${DISTDIR}/pcap.h ${DESTDIR}/usr/include/pcap.h || \
	  ( $(INSTALL) -c -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${DISTDIR}/pcap.h ${DESTDIR}/usr/include; )
	-cmp -s ${DISTDIR}/pcap-namedb.h ${DESTDIR}/usr/include/pcap-namedb.h || \
	  ( $(INSTALL) -c -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${DISTDIR}/pcap-namedb.h ${DESTDIR}/usr/include; )


scanner.o: tokdefs.h

tokdefs.h: grammar.c
	mv -f y.tab.h tokdefs.h

.include <bsd.lib.mk>
