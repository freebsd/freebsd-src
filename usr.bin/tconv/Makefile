# Makefile for tconv
# $Id: Makefile,v 1.1.1.1 1994/10/09 17:40:30 ache Exp $

PROG=   tconv
SRCS=   tconv.c quit.c
MLINKS= tconv.1 tic.1  tconv.1 captoinfo.1
LINKS=  ${BINDIR}/tconv ${BINDIR}/tic  ${BINDIR}/tconv ${BINDIR}/captoinfo
CFLAGS+= -I${.CURDIR}/../../lib/libmytinfo -Wall
DPADD= $(LIBMYTINFO)
LDADD= -lmytinfo

.include <bsd.prog.mk>
