#	$Id: Makefile,v 1.2 1995/09/07 20:50:44 mark Exp $

PROG=	kadmind
SRCS=	admin_server.c kadm_funcs.c kadm_ser_wrap.c kadm_server.c
CFLAGS+=-DPOSIX -I${.CURDIR}/../../include -I${.CURDIR}/../../lib/libkadm -Wall
DPADD=	${LIBKDB} ${LIBKRB} ${LIBDES}
LDADD+=	-lkadm -lkdb -lkrb -ldes -lacl -lcom_err
MAN8=	kadmind.8
BINDIR=	/usr/sbin

.include <bsd.prog.mk>
