# $FreeBSD$

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

.for lib in ${LIBDEPS}
DPADD+=		${OBJTOP}/lib/clang/lib${lib}/lib${lib}.a
LDADD+=		${OBJTOP}/lib/clang/lib${lib}/lib${lib}.a
.endfor

LLGSOptions.inc: ${LLDB_SRCS}/tools/lldb-server/LLGSOptions.td
	${LLVM_TBLGEN} ${LLVM_TBLGEN_OPTS} --gen-opt-parser-defs \
	    -I ${LLVM_SRCS}/include -d ${.TARGET}.d -o ${.TARGET} ${.ALLSRC}
TGHDRS+=	LLGSOptions.inc

${TGHDRS}: ${LLVM_TBLGEN_BIN}

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

.include <bsd.prog.mk>
