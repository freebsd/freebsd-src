# $Header: /a/cvs/386BSD/src/usr.bin/tn3270/tools/mkastosc/Makefile,v 1.1 1993/06/29 11:40:12 nate Exp $

CFLAGS += -I${.CURDIR}/../mkhits -I${.CURDIR}/.. -I.

NOMAN = I mean it!

SRCS = mkastosc.c dohits.c asc_ebc.c ebc_disp.c
PROG = mkastosc

.include <bsd.prog.mk>

.PATH: ${.CURDIR}/../mkhits ${.CURDIR}/../../api
