#	@(#)Makefile	8.1 (Berkeley) 6/4/93

LIB=	rkinit
CFLAGS+=-DKERBEROS -DCRYPT -DDEBUG -I${RKINITOBJDIR}
SRCS=	rkinit_err.c rk_lib.c rk_rpc.c rk_util.c rk_krb.c rkinit_err.c

LDADD+=	-lcom_err

beforeinstall:
	-cd ${.OBJDIR}; cmp -s rkinit_err.h \
	    ${DESTDIR}/usr/include/kerberosIV/rkinit_err.h || \
	    install -c -o ${BINOWN} -g ${BINGRP} -m 444 rkinit_err.h \
	    ${DESTDIR}/usr/include/kerberosIV

MAN3=	rkinit.3

.include <bsd.lib.mk>

rkinit_err.c:	${RKINITOBJDIR}/rkinit_err.h
