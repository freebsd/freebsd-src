#	$Id$

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
		-L${KDBOBJDIR} -lkdb -L${KRBOBJDIR} -lkrb -ldes
DPADD=		${LIBROKEN} ${LIBKDB} ${LIBKRB} \
		${LIBDES}

.include <bsd.prog.mk>

.PATH:	${KRB4DIR}/admin
