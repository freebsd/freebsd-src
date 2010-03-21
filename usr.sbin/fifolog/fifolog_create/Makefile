# $FreeBSD: src/usr.sbin/fifolog/fifolog_create/Makefile,v 1.1.2.1.6.1 2010/02/10 00:26:20 kensmith Exp $

PROG =		fifolog_create

CFLAGS +=	-I${.CURDIR}/../lib

DPADD =		${LIBFIFOLOG} ${LIBUTIL}
LDADD =		${LIBFIFOLOG} -lutil

MAN	=	fifolog.1
MLINKS	+=	fifolog.1 fifolog_create.1
MLINKS	+=	fifolog.1 fifolog_reader.1
MLINKS	+=	fifolog.1 fifolog_writer.1

.include <bsd.prog.mk>

test:	${PROG}
	rm -f /tmp/fifolog.?
	./${PROG} /tmp/fifolog.0
	./${PROG} -s 10m /tmp/fifolog.1
	./${PROG} -l 1k /tmp/fifolog.2
	./${PROG} -r 1k /tmp/fifolog.3
