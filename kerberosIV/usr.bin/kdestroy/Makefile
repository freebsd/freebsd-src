#	$Id: Makefile,v 1.1.1.1 1997/09/24 20:37:14 markm Exp $

PROG=		kdestroy
MAN1=		${KRB4DIR}/man/kdestroy.1
SRCS=		kdestroy.c
CFLAGS+=	-I${KRB4DIR}/include \
		-I${KRB4DIR}/lib/roken \
		-I${KRB4DIR}/lib/kafs \
		-I${KRB4DIR}/lib/kadm \
		-I${KRB4DIR}/lib/kdb \
		-I${KRB4DIR}/lib/krb \
		-I${KRB4DIR}/kuser
LDADD=		-L${ROKENOBJDIR} -lroken \
		-L${KAFSOBJDIR} -lkafs -L${KRBOBJDIR} -lkrb -ldes -lcrypt
DPADD=		${LIBROKEN} ${LIBKAFS} ${LIBKRB} ${LIBDES} ${LIBCRYPT}

.include <bsd.prog.mk>

.PATH:	${KRB4DIR}/kuser
