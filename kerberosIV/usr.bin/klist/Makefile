#	$Id$

PROG=		klist
MAN1=		${KRB4DIR}/man/klist.1
SRCS=		klist.c
CFLAGS+=	-I${KRB4DIR}/include \
		-I${KRB4DIR}/lib/roken \
		-I${KRB4DIR}/lib/kafs \
		-I${KRB4DIR}/lib/kadm \
		-I${KRB4DIR}/lib/kdb \
		-I${KRB4DIR}/lib/krb \
		-I${KRB4DIR}/kuser
LDADD=		-L${ROKENOBJDIR} -lroken \
		-L${KAFSOBJDIR} -lkafs -L${KRBOBJDIR} -lkrb -ldes
DPADD=		${LIBROKEN} ${LIBKAFS} ${LIBKRB} ${LIBDES}

.include <bsd.prog.mk>

.PATH:	${KRB4DIR}/kuser
