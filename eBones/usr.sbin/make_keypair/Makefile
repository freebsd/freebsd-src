#	@(#)Makefile	8.1 (Berkeley) 6/1/93
#	$id$

PROG=	make_keypair
MAN8=	make_keypair.8
CFLAGS+=-DKERBEROS -I${.CURDIR}/../../usr.bin/register
DPADD=	${LIBKDB} ${LIBKRB} ${LIBDES}
LDADD=	-lkdb -lkrb -ldes

.include <bsd.prog.mk>
