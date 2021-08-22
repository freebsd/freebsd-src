# $FreeBSD$

.include "${SRCTOP}/lib/clang/lldb.pre.mk"

PACKAGE=	lldb
PROG_CXX=	lldb

CFLAGS+=	-I${LLDB_SRCS}/include
CFLAGS+=	-I${.OBJDIR}

SRCDIR=		lldb/tools/driver
SRCS+=		Driver.cpp

.include "${SRCTOP}/lib/clang/clang.build.mk"

LIBDEPS+=	lldb
LIBDEPS+=	clang
LIBDEPS+=	llvm

.for lib in ${LIBDEPS}
DPADD+=		${OBJTOP}/lib/clang/lib${lib}/lib${lib}.a
LDADD+=		${OBJTOP}/lib/clang/lib${lib}/lib${lib}.a
.endfor

INCFILE=	Options.inc
TDFILE=		${LLVM_BASE}/${SRCDIR}/Options.td
GENOPT=		-gen-opt-parser-defs
${INCFILE}: ${TDFILE}
	${LLVM_TBLGEN} ${GENOPT} -I ${LLVM_SRCS}/include -d ${.TARGET:C/$/.d/} \
	    -o ${.TARGET} ${TDFILE}
TGHDRS+=	${INCFILE}

DEPENDFILES+=	${TGHDRS:C/$/.d/}
DPSRCS+=	${TGHDRS}
CLEANFILES+=	${TGHDRS} ${TGHDRS:C/$/.d/}

LIBADD=		tinfow
LIBADD+=	edit
LIBADD+=	execinfo
LIBADD+=	lua
LIBADD+=	lzma
LIBADD+=	ncursesw
LIBADD+=	panelw
LIBADD+=	pthread
LIBADD+=	z

.include <bsd.prog.mk>
