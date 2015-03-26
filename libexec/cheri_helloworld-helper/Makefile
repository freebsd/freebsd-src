#
# $FreeBSD$
#
PROG=	cheri_helloworld-helper
SRCS=	cheri_helloworld-helper.c

INCS=	cheri_helloworld-helper.h

LDFLAGS+=	-Wl,--script=${.CURDIR}/../../lib/libc_cheri/sandbox.ld \
		-nostdlib
MAN=
LIBADD=	c_cheri

NEED_CHERI=	pure
WANT_DUMP=	yes

NO_SHARED=	yes

.include <bsd.prog.mk>
