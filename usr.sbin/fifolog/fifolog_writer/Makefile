# $FreeBSD: src/usr.sbin/fifolog/fifolog_writer/Makefile,v 1.2.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $

PROG=	fifolog_writer

CFLAGS+= -I${.CURDIR}/../lib

NO_MAN=	# see ../fifolog_create/fifolog.1

DPADD=	${LIBFIFOLOG} ${LIBUTIL} ${LIBZ}
LDADD=	${LIBFIFOLOG} -lutil -lz

regress:
	date | ./${PROG} -z 0 /tmp/fifolog.0
	lptest 65 | ./${PROG} -z 9 /tmp/fifolog.1

.include <bsd.prog.mk>
