# Makefile for libpcap
# $Id: Makefile,v 1.18 1997/10/10 11:55:42 peter Exp $

LIB=    pcap
SRCS=	grammar.y tokdefs.h pcap-bpf.c \
        pcap.c inet.c gencode.c optimize.c nametoaddr.c \
	etherent.c savefile.c bpf_filter.c bpf_image.c \
        scanner.l
MAN3=	pcap.3
CLEANFILES+= tokdefs.h

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
.for i in pcap.h pcap-int.h pcap-namedb.h
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 ${PCAP_DISTDIR}/$i \
		${DESTDIR}/usr/include
.endfor

tokdefs.h: grammar.h
	ln -sf grammar.h tokdefs.h

.include <bsd.lib.mk>
