# bootpd/Makefile
# $Id: Makefile,v 1.1.1.1 1994/06/27 21:25:49 gwr Exp $

PROG=	bootpd
CFLAGS+= -DETC_ETHERS -DSYSLOG -DDEBUG -DVEND_CMU

SRCS=	bootpd.c dovend.c readfile.c hash.c dumptab.c \
	 lookup.c getif.c hwaddr.c report.c tzone.c

MAN5=	bootptab.0
MAN8=	bootpd.0
MLINKS= bootpd.8 bootpgw.8

.include <bsd.prog.mk>
