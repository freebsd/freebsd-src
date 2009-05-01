# $FreeBSD: src/usr.sbin/fifolog/fifolog_reader/Makefile,v 1.2.2.1.4.1 2009/04/15 03:14:26 kensmith Exp $

PROG =		fifolog_reader

CFLAGS +=	-I${.CURDIR}/../lib  -I${.CURDIR}

NO_MAN =	see ../fifolog_create/fifolog.1

DPADD =		${LIBFIFOLOG} ${LIBUTIL} ${LIBZ}
LDADD =		${LIBFIFOLOG} -lutil -lz

.include <bsd.prog.mk>

test:	${PROG}
	./${PROG} /tmp/fifolog.0
	./${PROG} -t /tmp/fifolog.0
	./${PROG} /tmp/fifolog.1
	./${PROG} -B "00:00" /tmp/fifolog.1

t2:
	./${PROG} -t /critter/10.1.29.74.fifolog  
