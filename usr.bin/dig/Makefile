#	@(#)Makefile	8.1 (Berkeley) 6/6/93

PROG=	dig
MAN1=	dig.1
SRCS=	dig.c debug.c list.c send.c subr.c

NSLOOKUP=${.CURDIR}/../../usr.sbin/nslookup
CFLAGS+=-I${NSLOOKUP}
.PATH:	 ${NSLOOKUP}

.include <bsd.prog.mk>
