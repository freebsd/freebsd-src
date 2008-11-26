# bootpd/Makefile
# $FreeBSD: src/libexec/bootpd/Makefile,v 1.9.30.1 2008/10/02 02:57:24 kensmith Exp $

PROG=	bootpd
CFLAGS+= -DETC_ETHERS
CFLAGS+= -DSYSLOG -DDEBUG -DVEND_CMU

SUBDIR= bootpgw tools

SRCS=	bootpd.c dovend.c readfile.c hash.c dumptab.c \
	 lookup.c getif.c hwaddr.c report.c tzone.c rtmsg.c

MAN=	bootptab.5 bootpd.8
MLINKS=	bootpd.8 bootpgw.8

.include <bsd.prog.mk>
