# bootpgw/Makefile
# $Id: Makefile,v 1.1.1.1 1994/06/27 21:29:46 gwr Exp $

PROG=	bootpgw
SRCDIR= ${.CURDIR}/../bootpd
CFLAGS+= -DSYSLOG -DDEBUG -I${SRCDIR}
.PATH: ${SRCDIR}

SRCS=	bootpgw.c getif.c hwaddr.c report.c

MAN8=

.include <bsd.prog.mk>

