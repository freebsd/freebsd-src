# Makefile for libpcap
# $FreeBSD$

LIB=    pcap
SRCS=	grammar.y tokdefs.h pcap-bpf.c \
	pcap.c inet.c gencode.c optimize.c nametoaddr.c \
	etherent.c savefile.c bpf_filter.c bpf_image.c bpf_dump.c \
	scanner.l version.c
MAN=	pcap.3
CLEANFILES=tokdefs.h version.c

DEFS=	-DHAVE_CONFIG_H
.if !defined(NOINET6)
DEFS+=	-DINET6
.endif

CFLAGS+=-I. -Dyylval=pcap_lval ${DEFS}

SHLIB_MAJOR=2
SHLIB_MINOR=5

#
# Magic to grab sources out of src/contrib
#
PCAP_DISTDIR?=${.CURDIR}/../../contrib/libpcap
CFLAGS+=-I${PCAP_DISTDIR} -I${PCAP_DISTDIR}/lbl -I${PCAP_DISTDIR}/bpf
.PATH:	${PCAP_DISTDIR}
.PATH:	${PCAP_DISTDIR}/bpf/net

version.c: ${PCAP_DISTDIR}/VERSION
	@rm -f $@
	sed 's/.*/char pcap_version[] = "&";/' ${PCAP_DISTDIR}/VERSION > $@

beforeinstall:
.for i in pcap.h pcap-int.h pcap-namedb.h
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 ${PCAP_DISTDIR}/$i \
		${DESTDIR}/usr/include
.endfor

tokdefs.h: grammar.h
	ln -sf grammar.h tokdefs.h

.include <bsd.lib.mk>
