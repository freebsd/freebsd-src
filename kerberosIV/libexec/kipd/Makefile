#	$Id: Makefile,v 1.1.1.1 1997/09/24 20:37:13 markm Exp $

PROG=		kipd
NOMAN=		yes
SRCS=		kipd.c common.c
CFLAGS+=	-I${KRB4DIR}/include \
		-I${KRB4DIR}/lib/roken \
		-I${KRB4DIR}/lib/krb \
		-I${KRB4DIR}/appl/kip
LDADD=		-L${ROKENOBJDIR} -lroken -L${KRBOBJDIR} -lkrb -ldes -lcrypt
DPADD=		${LIBROKEN} ${LIBKRB} ${LIBDES} ${LIBCRYPT}

.include <bsd.prog.mk>

.PATH:	${KRB4DIR}/appl/kip
