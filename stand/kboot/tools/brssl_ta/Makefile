.include <bsd.init.mk>

PROG=	brssl_ta
MAN=
BINDIR?=	/usr/bin

.include "${SRCTOP}/lib/libbearssl/Makefile.inc"

.PATH:	${BEARSSL_TOOLS}
.PATH:	${BEARSSL_SRC}/codec
.PATH:	${BEARSSL_SRC}/x509

SRCS=	brssl_ta.c
SRCS+=	ta.c certs.c files.c xmem.c vector.c
SRCS+=	pemdec.c x509_decoder.c

CFLAGS+=	-I${BEARSSL_SRC}
CFLAGS+=	-I${BEARSSL_TOOLS}
CFLAGS.certs.c+=	-Wno-cast-qual

.include <bsd.prog.mk>
