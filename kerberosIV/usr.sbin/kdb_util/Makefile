#	$Id: Makefile,v 1.2 1997/10/02 15:03:22 markm Exp $

PROG=		kdb_util
MAN8=		${KRB4DIR}/man/kdb_util.8
SRCS=		kdb_util.c
CFLAGS+=	-I${KRB4DIR}/include \
		-I${KRB4DIR}/lib/roken \
		-I${KRB4DIR}/lib/kadm \
		-I${KRB4DIR}/lib/kdb \
		-I${KRB4DIR}/lib/krb \
		-I${KRB4DIR}/admin \
		-I${.CURDIR}/../include
LDADD=		-L${ROKENOBJDIR} -lroken \
		-L${KDBOBJDIR} -lkdb -L${KRBOBJDIR} -lkrb -ldes -lcrypt
DPADD=		${LIBROKEN} ${LIBKDB} ${LIBKRB} ${LIBDES} ${LIBCRYPT}

.include <bsd.prog.mk>

.PATH:	${KRB4DIR}/admin
