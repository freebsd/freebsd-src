#	$Id$

PROG=		kip
NOMAN=		true
SRCS=		kip.c common.c
CFLAGS+=	-I${KRB4DIR}/include \
		-I${KRB4DIR}/lib/roken \
		-I${KRB4DIR}/lib/krb \
		-I${KRB4DIR}/kadmin \
		-I${KRBOBJDIR} \
		-I${.CURDIR}/../include
LDADD=		-L${ROKENOBJDIR} -lroken -L${KRBOBJDIR} -lkrb -ldes
DPADD=		${LIBROKEN} ${LIBKRB} ${LIBDES}

.include <bsd.prog.mk>

.PATH:	${KRB4DIR}/appl/kip
