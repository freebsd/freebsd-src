NEED_CHERI=	pure

LIB=		vuln_png_cheri

CFLAGS+=	-DCTSRD_BACKDOOR -DBROKEN_SYSCALLS

BUILDDIR=	${.CURDIR}/../../../lib/libpng
SRCDIR=		${.CURDIR}/../../../contrib/libpng

.include <${BUILDDIR}/Makefile>
