# Makefile
# $Id: Makefile,v 1.3 1997/02/22 14:21:15 peter Exp $

PROG=	bootpgw
NOMAN=  true
SRCS=	bootpgw.c getif.c hwaddr.c report.c rtmsg.c

SRCDIR=	${.CURDIR}/..
CFLAGS+=-I${SRCDIR}
.PATH:	${SRCDIR}

.include <bsd.prog.mk>
