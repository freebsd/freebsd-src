#	@(#)Makefile	8.1 (Berkeley) 6/4/93

PROG=	rlogind
CFLAGS+=-DKERBEROS -DCRYPT
SRCS=	rlogind.c des_rw.c
MAN8=	rlogind.0
DPADD=	${LIBUTIL} ${LIBKRB} ${LIBDES}
LDADD=	-lutil -lkrb -ldes
.PATH: ${.CURDIR}/../../usr.bin/rlogin

.include <bsd.prog.mk>
