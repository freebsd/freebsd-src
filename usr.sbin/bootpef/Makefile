# bootpef/Makefile
# $Id: Makefile,v 1.2 1994/09/10 15:00:04 csgr Exp $

PROG=	bootpef
SRCDIR= ${.CURDIR}/../bootpd
#CFLAGS+= -DETC_ETHERS 
CFLAGS+= -DDEBUG -I${SRCDIR}
.PATH: ${SRCDIR}
MANSRC= ${SRCDIR}

SRCS=	bootpef.c dovend.c readfile.c hash.c dumptab.c \
	 lookup.c hwaddr.c report.c rtmsg.c tzone.c

MAN8=	bootpef.8

.include <bsd.prog.mk>

