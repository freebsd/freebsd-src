# Makefile for libpcap
# $Id: Makefile,v 1.15 1997/02/22 15:07:50 peter Exp $

LIB=    pcap
SRCS=	grammar.c tokdefs.h pcap-bpf.c \
        pcap.c inet.c gencode.c optimize.c nametoaddr.c \
	etherent.c savefile.c bpf_filter.c bpf_image.c \
        scanner.c
MAN3=	pcap.3
CLEANFILES+= tokdefs.h grammar.c scanner.c y.tab.c y.tab.h

DEFS=	-DHAVE_SYS_IOCCOM_H=1 -DHAVE_SYS_SOCKIO_H=1 \
	-DHAVE_ETHER_HOSTTON=1 -DHAVE_STRERROR=1 \
	-DHAVE_SOCKADDR_SA_LEN=1 -DLBL_ALIGN=1 

CFLAGS+=-I. -Dyylval=pcap_lval ${DEFS}

SHLIB_MAJOR=2
SHLIB_MINOR=2

#
# Magic to grab sources out of src/contrib
#
PCAP_DISTDIR?=${.CURDIR}/../../contrib/libpcap
CFLAGS+=-I${PCAP_DISTDIR} -I${PCAP_DISTDIR}/lbl -I${PCAP_DISTDIR}/bpf
.PATH:	${PCAP_DISTDIR}
.PATH:	${PCAP_DISTDIR}/bpf/net

beforeinstall:
.for i in pcap.h pcap-namedb.h
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 ${PCAP_DISTDIR}/$i \
		${DESTDIR}/usr/include
.endfor

tokdefs.h grammar.c: grammar.y
	${YACC} ${YACCFLAGS} -d ${PCAP_DISTDIR}/grammar.y
	mv y.tab.c grammar.c
	mv y.tab.h tokdefs.h

.include <bsd.lib.mk>
