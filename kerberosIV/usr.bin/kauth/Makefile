#	$Id: Makefile,v 1.1.1.1 1997/09/24 20:37:13 markm Exp $

PROG=		kauth
MAN1=		${KRB4DIR}/man/kauth.1
SRCS=		kauth.c rkinit.c encdata.c marshall.c
CFLAGS+=	-I${KRB4DIR}/include \
		-I${KRB4DIR}/lib/roken \
		-I${KRB4DIR}/lib/kafs \
		-I${KRB4DIR}/lib/krb \
		-I${KRB4DIR}/appl/kauth
LDADD=		-L${ROKENOBJDIR} -lroken -L${KAFSOBJDIR} -lkafs \
		-L${KRBOBJDIR} -lkrb -ldes -lcrypt
DPADD=		${LIBROKEN} ${LIBKAFS} ${LIBKRB} ${LIBDES} ${LIBCRYPT}

.include <bsd.prog.mk>

.PATH:	${KRB4DIR}/appl/kauth
