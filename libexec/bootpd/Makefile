# bootpd/Makefile
# $FreeBSD: src/libexec/bootpd/Makefile,v 1.8 1999/08/28 00:09:14 peter Exp $

PROG=	bootpd
CFLAGS+= -DETC_ETHERS
CFLAGS+= -DSYSLOG -DDEBUG -DVEND_CMU

SUBDIR= bootpgw tools

SRCS=	bootpd.c dovend.c readfile.c hash.c dumptab.c \
	 lookup.c getif.c hwaddr.c report.c tzone.c rtmsg.c

MAN5=	bootptab.5
MAN8=	bootpd.8
MLINKS=	bootpd.8 bootpgw.8

.include <bsd.prog.mk>
