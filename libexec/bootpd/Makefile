# bootpd/Makefile
# $Id: Makefile,v 1.5 1996/02/02 17:48:33 mpp Exp $

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
