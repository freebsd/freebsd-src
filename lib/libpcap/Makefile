# Makefile for libpcap
# $Id: Makefile,v 1.12 1996/08/30 01:59:46 peter Exp $

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
PCAP_DISTDIR?=${.CURDIR}/../../contrib/libpcap
CFLAGS+=-I${PCAP_DISTDIR} -I${PCAP_DISTDIR}/lbl
.PATH:	${PCAP_DISTDIR}
.PATH:	${PCAP_DISTDIR}/bpf/net

beforeinstall:
.for i in pcap.h pcap-namedb.h
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 ${PCAP_DISTDIR}/$i \
		${DESTDIR}/usr/include
.endfor

scanner.o: tokdefs.h

.ORDER: grammar.c tokdefs.h
tokdefs.h: grammar.c
	mv -f y.tab.h tokdefs.h

.include <bsd.lib.mk>
