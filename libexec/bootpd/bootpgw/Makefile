# Makefile
# $Id: Makefile,v 1.1.1.1 1994/09/30 05:43:13 pst Exp $

PROG=	bootpgw
NOMAN=  true
SRCS=	bootpgw.c getif.c hwaddr.c report.c rtmsg.c

SRCDIR=	${.CURDIR}/..
CFLAGS+=-I${SRCDIR}
.PATH:	${SRCDIR}

.include <bsd.prog.mk>
