# Makefile for libpcap
# $Id: Makefile,v 1.8 1994/11/16 11:54:09 ache Exp $

LIB=    pcap

SRCS=	\
        pcap-bpf.c \
        pcap.c inet.c gencode.c optimize.c nametoaddr.c etherent.c savefile.c \
        bpf_filter.c bpf_image.c \
        grammar.y \
        scanner.l

CLEANFILES+= lex.yy.c tokdefs.h grammar.c scanner.c
CFLAGS+=-Wall -I. -I${.CURDIR} -DFDDI 
MAN3=	pcap.3

beforeinstall:
	-cmp -s ${.CURDIR}/pcap.h ${DESTDIR}/usr/include/pcap.h.h || \
	  ( $(INSTALL) $(COPY) -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${.CURDIR}/pcap.h ${DESTDIR}/usr/include; )
	-cmp -s ${.CURDIR}/pcap-namedb.h ${DESTDIR}/usr/include/pcap-namedb.h || \
	  ( $(INSTALL) $(COPY) -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${.CURDIR}/pcap-namedb.h ${DESTDIR}/usr/include; )


scanner.c: scanner.l
	rm -f $@
	$(LEX) -t $< > $@

scanner.o: scanner.c
	rm -f $@; $(CC) $(CFLAGS) $(LEX_DEFINES) -c $*.c

tokdefs.h: grammar.c

grammar.c: grammar.y
	rm -f grammar.c tokdefs.h
	$(YACC) -d $<
	mv y.tab.c grammar.c
	mv y.tab.h tokdefs.h

grammar.o: grammar.c
	$(CC) -c $(CFLAGS) -Dyylval=pcap_lval grammar.c


.include <bsd.lib.mk>
