#	$Id$

PROG=		kauthd
MAN8=		${KRB4DIR}/man/kauthd.8
SRCS=		kauthd.c encdata.c marshall.c
CFLAGS+=	-I${KRB4DIR}/include \
		-I${KRB4DIR}/lib/roken \
		-I${KRB4DIR}/lib/kafs \
		-I${KRB4DIR}/lib/krb \
		-I${KRB4DIR}/appl/kauth
LDADD=		-L${ROKENOBJDIR} -lroken -L${KRBOBJDIR} -lkrb -ldes
DPADD=		${LIBROKEN} ${LIBKRB} ${LIBDES}

.include <bsd.prog.mk>

.PATH:	${KRB4DIR}/appl/kauth
