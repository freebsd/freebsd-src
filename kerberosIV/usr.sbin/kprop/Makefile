#	$Id: Makefile,v 1.1.1.1 1997/09/24 20:37:14 markm Exp $

PROG=		kprop
MAN8=		${KRB4DIR}/man/kprop.8
SRCS=		kprop.c
CFLAGS+=	-I${KRB4DIR}/include \
		-I${KRB4DIR}/lib/roken \
		-I${KRB4DIR}/lib/kdb \
		-I${KRB4DIR}/lib/krb \
		-I${KRB4DIR}/slave \
		-I${INCLOBJDIR} \
		-I${.CURDIR}/../include
LDADD=		-L${ROKENOBJDIR} -lroken -L${KRBOBJDIR} -lkrb -ldes -lcrypt
DPADD=		${LIBROKEN} ${LIBKRB} ${LIBDES} ${LIBCRYPT}

.include <bsd.prog.mk>

.PATH:	${KRB4DIR}/slave
