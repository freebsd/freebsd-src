#	@(#)Makefile	8.1 (Berkeley) 6/1/93

PROG=	make_keypair
MAN8=	make_keypair.8
CFLAGS+=-DKERBEROS -I${.CURDIR}/../include -I${.CURDIR}/../register -Wall
DPADD=	${LIBKRB}
LDADD=	-L${KDBOBJDIR} -lkdb -L${KRBOBJDIR} -lkrb -ldes

.include <bsd.prog.mk>
