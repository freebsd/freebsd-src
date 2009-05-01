#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.sbin/timed/timed/Makefile,v 1.5.34.1 2009/04/15 03:14:26 kensmith Exp $
#
# optional flags are: MEASURE TESTING DEBUG

PROG=	timed
MAN=	timed.8
SRCS=	acksend.c candidate.c correct.c master.c networkdelta.c readmsg.c \
	slave.c timed.c byteorder.c measure.c cksum.c

DPADD=	${LIBUTIL}
LDADD=	-lutil

.include "../../Makefile.inc"
.include <bsd.prog.mk>
