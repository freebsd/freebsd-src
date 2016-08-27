# $FreeBSD$

LLVM_SRCS=	${SRCTOP}/contrib/llvm
CLANG_SRCS=	${LLVM_SRCS}/tools/clang

CFLAGS+=	-I${OBJTOP}/lib/clang/libclang
CFLAGS+=	-I${OBJTOP}/lib/clang/libllvm

.include "${SRCTOP}/lib/clang/clang.build.mk"

LIBDEPS+=	clang
LIBDEPS+=	llvm

.for lib in ${LIBDEPS}
DPADD+=		${OBJTOP}/lib/clang/lib${lib}/lib${lib}.a
LDADD+=		${OBJTOP}/lib/clang/lib${lib}/lib${lib}.a
.endfor

LIBADD+=	ncursesw
LIBADD+=	pthread

.include <bsd.prog.mk>
