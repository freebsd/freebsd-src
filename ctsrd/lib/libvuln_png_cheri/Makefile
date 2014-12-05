USE_CHERI=yes
USE_CHERI_STACK=yes

LIB=		vuln_png_cheri

CFLAGS+=	-DCTSRD_BACKDOOR

BUILDDIR=	${.CURDIR}/../../../lib/libpng
SRCDIR=		${.CURDIR}/../../../contrib/libpng

.include <${BUILDDIR}/Makefile>
