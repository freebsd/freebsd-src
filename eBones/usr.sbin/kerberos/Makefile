#	From: @(#)Makefile	5.1 (Berkeley) 6/25/90
#	$FreeBSD$

PROG=	kerberos
SRCS=	kerberos.c cr_err_reply.c
CFLAGS+=-DKERBEROS -DDEBUG
DPADD=	${LIBKDB} ${LIBKRB} ${LIBDES}
LDADD=	-L${KDBOBJDIR} -lkdb -L${KRBOBJDIR} -lkrb -L${DESOBJDIR} -ldes
NOMAN=	noman

.include <bsd.prog.mk>
