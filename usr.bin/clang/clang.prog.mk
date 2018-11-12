# $FreeBSD$

LLVM_SRCS= ${.CURDIR}/../../../contrib/llvm

.include "../../lib/clang/clang.build.mk"

.for lib in ${LIBDEPS}
DPADD+=	${.OBJDIR}/../../../lib/clang/lib${lib}/lib${lib}.a
LDADD+=	${.OBJDIR}/../../../lib/clang/lib${lib}/lib${lib}.a
.endfor

PACKAGE=	clang

LIBADD+= ncursesw pthread

BINDIR?= /usr/bin


.if ${MK_SHARED_TOOLCHAIN} == "no"
NO_SHARED= yes
.endif

.include <bsd.prog.mk>
