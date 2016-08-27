# $FreeBSD$

LLVM_SRCS=	${SRCTOP}/contrib/llvm
CLANG_SRCS=	${LLVM_SRCS}/tools/clang

CFLAGS+=	-I${.OBJDIR}/../../../lib/clang/libclang
CFLAGS+=	-I${.OBJDIR}/../../../lib/clang/libllvm

.include "${SRCTOP}/lib/clang/clang.build.mk"

LIBDEPS+=	clang
LIBDEPS+=	llvm

.for lib in ${LIBDEPS}
DPADD+=		${.OBJDIR}/../../../lib/clang/lib${lib}/lib${lib}.a
LDADD+=		${.OBJDIR}/../../../lib/clang/lib${lib}/lib${lib}.a
.endfor

LIBADD+=	ncursesw
LIBADD+=	pthread

.include <bsd.prog.mk>
