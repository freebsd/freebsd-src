# Makefile for libpcap
# $Id: Makefile,v 1.7 1996/06/02 17:09:03 phk Exp $

LIB=    pcap

SRCS=	\
        pcap-bpf.c \
        pcap.c inet.c gencode.c optimize.c nametoaddr.c etherent.c savefile.c \
        bpf_filter.c bpf_image.c \
        grammar.y scanner.l

CLEANFILES+= lex.yy.c tokdefs.h grammar.c scanner.c
# CFLAGS+=-Wall -I. -I${.CURDIR} -DFDDI -Dyylval=pcap_lval
CFLAGS+=-DFDDI -I. -I${.CURDIR} -DETHER_SERVICE
MAN3=	pcap.3

beforeinstall:
	-cmp -s ${.CURDIR}/pcap.h ${DESTDIR}/usr/include/pcap.h || \
	  ( $(INSTALL) -c -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${.CURDIR}/pcap.h ${DESTDIR}/usr/include; )
	-cmp -s ${.CURDIR}/pcap-namedb.h ${DESTDIR}/usr/include/pcap-namedb.h || \
	  ( $(INSTALL) -c -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${.CURDIR}/pcap-namedb.h ${DESTDIR}/usr/include; )

scanner.o: tokdefs.h

tokdefs.h: grammar.c
	mv -f y.tab.h tokdefs.h


.include <bsd.lib.mk>
