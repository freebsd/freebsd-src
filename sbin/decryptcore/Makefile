
PROG=	decryptcore

CFLAGS+=-DOPENSSL_API_COMPAT=0x10100000L
LIBADD=	crypto pjdlog

MAN=	decryptcore.8

CFLAGS+=-I${SRCTOP}/lib/libpjdlog

.include <bsd.prog.mk>
