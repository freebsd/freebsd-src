PROG=	ncrcontrol
SRCS=	ncrcontrol.c

CFLAGS+=	-I/sys
LDADD+=		-lkvm
DPADD+=		${DESTDIR}/usr/lib/libkvm.a

.include <bsd.prog.mk>
