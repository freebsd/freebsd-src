# bootpgw/Makefile
# Makefile,v 1.2 1994/09/12 07:48:33 csgr Exp

PROG=	bootpgw
SRCDIR= ${.CURDIR}/../bootpd
CFLAGS+= -DSYSLOG -DDEBUG -I${SRCDIR}
.PATH: ${SRCDIR}

SRCS=	bootpgw.c getif.c hwaddr.c report.c rtmsg.c

MAN8=

.include <bsd.prog.mk>

