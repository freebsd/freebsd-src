# Makefile for tconv
# $FreeBSD: src/usr.bin/tconv/Makefile,v 1.6 2001/12/12 00:06:51 markm Exp $

PROG=   tconv
SRCS=   tconv.c quit.c
MLINKS= tconv.1 tic.1  tconv.1 captoinfo.1
LINKS=  ${BINDIR}/tconv ${BINDIR}/tic  ${BINDIR}/tconv ${BINDIR}/captoinfo
CFLAGS+= -I${.CURDIR}/../../lib/libmytinfo
DPADD= $(LIBMYTINFO)
LDADD= -lmytinfo

.include <bsd.prog.mk>
