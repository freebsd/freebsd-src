#	$Id: Makefile,v 1.14 1998/05/04 16:22:30 bde Exp $

.PATH:	${.CURDIR}/../../../contrib/bc/bc \
	${.CURDIR}/../../../contrib/bc/lib \
	${.CURDIR}/../../../contrib/bc/doc

PROG=	bc
SRCS=	bc.y execute.c global.c load.c main.c scan.l storage.c util.c \
	number.c getopt.c getopt1.c
CFLAGS+=-I. -I${.CURDIR} -I${.CURDIR}/../../../contrib/bc/h -DHAVE_CONFIG_H
DPADD=	${LIBREADLINE} ${LIBTERMCAP}
LDADD=	-lreadline -ltermcap

.include <bsd.prog.mk>
