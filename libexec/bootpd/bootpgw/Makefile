# Makefile
# $Id: Makefile,v 1.1 1996/01/23 02:02:03 pst Exp $

PROG=	bootpgw
NOMAN=  true
SRCS=	bootpgw.c getif.c hwaddr.c report.c rtmsg.c

SRCDIR=	${.CURDIR}/..
CFLAGS+=-I${SRCDIR}
.PATH:	${SRCDIR}

.include <bsd.prog.mk>
