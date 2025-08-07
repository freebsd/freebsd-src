
.include "${SRCTOP}/lib/clang/llvm.pre.mk"

CFLAGS+=	-I${OBJTOP}/lib/clang/libllvm

.include "${SRCTOP}/lib/clang/llvm.build.mk"

# Special case for the bootstrap-tools phase.
.if (defined(TOOLS_PREFIX) || ${MACHINE} == "host") && \
    (${PROG_CXX} == "clang-tblgen" || ${PROG_CXX} == "lldb-tblgen" || \
     ${PROG_CXX} == "llvm-min-tblgen" || ${PROG_CXX} == "llvm-tblgen")
LIBDEPS+=	llvmminimal
LIBPRIV=
LIBEXT=		a
.else
LIBDEPS+=	llvm
LIBPRIV=	private
LIBEXT=		so
LIBADD+=	z
LIBADD+=	zstd
.endif

.for lib in ${LIBDEPS}
DPADD+=		${OBJTOP}/lib/clang/lib${lib}/lib${LIBPRIV}${lib}.${LIBEXT}
LDADD+=		${OBJTOP}/lib/clang/lib${lib}/lib${LIBPRIV}${lib}.${LIBEXT}
.endfor

PACKAGE?=	clang

.if ${.MAKE.OS} == "FreeBSD" || !defined(BOOTSTRAPPING)
LIBADD+=	execinfo
LIBADD+=	tinfow
.endif
LIBADD+=	pthread

.include <bsd.prog.mk>
