.include "${SRCTOP}/lib/clang/lldb.pre.mk"

PACKAGE=	lldb
PROG_CXX=	lldb-server

CFLAGS+=	-I${LLDB_SRCS}/include
CFLAGS+=	-I${LLDB_SRCS}/source
CFLAGS+=	-I${.OBJDIR}

SRCDIR=		lldb/tools/lldb-server

SRCS+=		Acceptor.cpp
SRCS+=		LLDBServerUtilities.cpp
SRCS+=		SystemInitializerLLGS.cpp
SRCS+=		lldb-gdbserver.cpp
SRCS+=		lldb-platform.cpp
SRCS+=		lldb-server.cpp

.include "${SRCTOP}/lib/clang/clang.build.mk"

LIBDEPS+=	lldb
LIBDEPS+=	clang
LIBDEPS+=	llvm

LIBPRIV=	private
LIBEXT=		so

.for lib in ${LIBDEPS}
DPADD+=		${OBJTOP}/lib/clang/lib${lib}/lib${LIBPRIV}${lib}.${LIBEXT}
LDADD+=		${OBJTOP}/lib/clang/lib${lib}/lib${LIBPRIV}${lib}.${LIBEXT}
.endfor

INCFILE=	LLGSOptions.inc
TDFILE=		${LLDB_SRCS}/tools/lldb-server/LLGSOptions.td
GENOPT=		-gen-opt-parser-defs
${INCFILE}: ${TDFILE}
	${LLVM_TBLGEN} ${GENOPT} -I ${LLVM_SRCS}/include -d ${.TARGET:C/$/.d/} \
	    -o ${.TARGET} ${TDFILE}
TGHDRS+=	${INCFILE}

DEPENDFILES+=	${TGHDRS:C/$/.d/}
DPSRCS+=	${TGHDRS}
CLEANFILES+=	${TGHDRS} ${TGHDRS:C/$/.d/}

LIBADD+=	tinfow
LIBADD+=	edit
LIBADD+=	execinfo
LIBADD+=	lua
LIBADD+=	lzma
LIBADD+=	ncursesw
LIBADD+=	panel
LIBADD+=	pthread
LIBADD+=	z
LIBADD+=	zstd

.include <bsd.prog.mk>
