# bootpgw/Makefile
# $Id: Makefile,v 1.1.1.1 1994/09/10 14:44:56 csgr Exp $

PROG=	bootpgw
SRCDIR= ${.CURDIR}/../bootpd
CFLAGS+= -DSYSLOG -DDEBUG -I${SRCDIR}
.PATH: ${SRCDIR}

SRCS=	bootpgw.c getif.c hwaddr.c report.c rtmsg.c

MAN8=

.include <bsd.prog.mk>

