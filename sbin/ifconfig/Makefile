#	@(#)Makefile	8.1 (Berkeley) 6/5/93

PROG=	ifconfig
MAN8=	ifconfig.8
DPADD=  ${LIBKVM} ${LIBIPX}
LDADD=  -lkvm -lipx
CFLAGS+=	-DNS

.include <bsd.prog.mk>
