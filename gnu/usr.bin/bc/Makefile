#	$Id: Makefile,v 1.12 1998/05/01 10:01:00 andreas Exp $

.PATH:	${.CURDIR}/../../../contrib/bc/bc \
	${.CURDIR}/../../../contrib/bc/lib \
	${.CURDIR}/../../../contrib/bc/doc

PROG=	bc
BCSRCS=	execute.c global.c load.c main.c storage.c util.c \
	number.c getopt.c getopt1.c
GENSRCS=bc.c scan.c
SRCS=	${GENSRCS} ${BCSRCS}

YACC=	bison -y

CFLAGS+=-I${.CURDIR} -I. -I${.CURDIR}/../../../contrib/bc/h -DHAVE_CONFIG_H

bc.h: bc.c
	mv y.tab.h bc.h

beforedepend: bc.h
scan.o:       bc.h

CLEANFILES+=    ${GENSRCS} y.tab.h bc.h

DPADD+= ${LIBREADLINE} ${LIBTERMCAP}
LDADD+= -lreadline -ltermcap

.include <bsd.prog.mk>
