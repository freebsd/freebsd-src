#	@(#)Makefile	5.11 (Berkeley) 6/29/90
#
# optional flags are: MEASURE TESTING DEBUG

PROG=	timed
SRCS=	acksend.c candidate.c correct.c master.c networkdelta.c readmsg.c \
	slave.c timed.c byteorder.c measure.c cksum.${MACHINE}.c
DPADD=	${LIBUTIL}
LDADD=	-lutil
MAN8=	timed.8
CLEANFILES+=	cksum.i386.c

cksum.i386.c:
	ln -s $(.CURDIR)/cksum.tahoe.c cksum.i386.c

.include "../../Makefile.inc"
.include <bsd.prog.mk>
