# bootpef/Makefile
# Makefile,v 1.3 1994/09/12 07:48:05 csgr Exp

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

