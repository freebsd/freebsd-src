# bootpd/Makefile
# $Id: Makefile,v 1.1.1.1 1994/09/30 05:45:04 pst Exp $

PROG=	bootpd
#CFLAGS+= -DETC_ETHERS
CFLAGS+= -DSYSLOG -DDEBUG -DVEND_CMU

SUBDIR= tools

SRCS=	bootpd.c dovend.c readfile.c hash.c dumptab.c \
	 lookup.c getif.c hwaddr.c report.c tzone.c rtmsg.c

MAN5=	bootptab.5
MAN8=	bootpd.8

.include <bsd.prog.mk>
