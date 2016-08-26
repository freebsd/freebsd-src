# $FreeBSD$

LLVM_SRCS=	${.CURDIR}/../../../contrib/llvm

CFLAGS+=	-I${.OBJDIR}/../../../lib/clang/libllvm

.include "${.CURDIR}/../../../lib/clang/llvm.build.mk"

# Special case for the bootstrap-tools phase.
.if defined(TOOLS_PREFIX) && \
    (${PROG_CXX} == "clang-tblgen" || ${PROG_CXX} == "llvm-tblgen")
LIBDEPS+=	llvmminimal
.else
LIBDEPS+=	llvm
.endif

.for lib in ${LIBDEPS}
DPADD+=		${.OBJDIR}/../../../lib/clang/lib${lib}/lib${lib}.a
LDADD+=		${.OBJDIR}/../../../lib/clang/lib${lib}/lib${lib}.a
.endfor

LIBADD+=	ncursesw
LIBADD+=	pthread

.include <bsd.prog.mk>
