# Makefile for tconv
# $FreeBSD: src/usr.bin/tconv/Makefile,v 1.4.2.1 1999/08/29 15:33:25 peter Exp $

PROG=   tconv
SRCS=   tconv.c quit.c
MLINKS= tconv.1 tic.1  tconv.1 captoinfo.1
LINKS=  ${BINDIR}/tconv ${BINDIR}/tic  ${BINDIR}/tconv ${BINDIR}/captoinfo
CFLAGS+= -I${.CURDIR}/../../lib/libmytinfo -Wall
DPADD= $(LIBMYTINFO)
LDADD= -lmytinfo

.include <bsd.prog.mk>
