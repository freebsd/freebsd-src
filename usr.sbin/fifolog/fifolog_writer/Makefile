# $FreeBSD: src/usr.sbin/fifolog/fifolog_writer/Makefile,v 1.1.2.1.4.1 2009/04/15 03:14:26 kensmith Exp $

PROG =		fifolog_writer

CFLAGS +=	-I${.CURDIR}/../lib

NO_MAN =	see ../fifolog_create/fifolog.1

DPADD =		${LIBFIFOLOG} ${LIBUTIL} ${LIBZ}
LDADD =		${LIBFIFOLOG} -lutil -lz

.include <bsd.prog.mk>

test:	${PROG}
	date | ./${PROG} -z 0 /tmp/fifolog.0
	lptest 65 | ./${PROG} -z 9 /tmp/fifolog.1
