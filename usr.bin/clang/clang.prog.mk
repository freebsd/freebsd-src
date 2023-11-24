
.include "${SRCTOP}/lib/clang/clang.pre.mk"

CFLAGS+=	-I${OBJTOP}/lib/clang/libclang
CFLAGS+=	-I${OBJTOP}/lib/clang/libllvm

.include "${SRCTOP}/lib/clang/clang.build.mk"

# Special case for the bootstrap-tools phase.
.if (defined(TOOLS_PREFIX) || ${MACHINE} == "host") && \
    ${PROG_CXX} == "clang-tblgen"
LIBDEPS+=	clangminimal
LIBDEPS+=	llvmminimal
.else
LIBDEPS+=	clang
LIBDEPS+=	llvm
LIBADD+=	z
LIBADD+=	zstd
.endif

.for lib in ${LIBDEPS}
DPADD+=		${OBJTOP}/lib/clang/lib${lib}/lib${lib}.a
LDADD+=		${OBJTOP}/lib/clang/lib${lib}/lib${lib}.a
.endfor

PACKAGE=	clang

.if ${.MAKE.OS} == "FreeBSD" || !defined(BOOTSTRAPPING)
LIBADD+=	execinfo
LIBADD+=	ncursesw
.endif
LIBADD+=	pthread

.include <bsd.prog.mk>
