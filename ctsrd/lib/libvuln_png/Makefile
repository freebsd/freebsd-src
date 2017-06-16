.include <src.opts.mk>

LIB=		vuln_png
LIBADD=		z

.if ${MK_DEMO_VULNERABILITIES} != "no"
CFLAGS+=	-DCTSRD_BACKDOOR
.else
SRCS+=	no_vuln_stub.c
.endif

BUILDDIR=	${SRCTOP}/lib/libpng

PNG_PRIVATE=no
.include <${BUILDDIR}/Makefile>
