# $FreeBSD$

LLVM_SRCS=	${.CURDIR}/../../../contrib/llvm
CLANG_SRCS=	${LLVM_SRCS}/tools/clang
LLDB_SRCS=	${LLVM_SRCS}/tools/lldb

PROG_CXX=	lldb
# Man page directory
.PATH:		${LLDB_SRCS}/docs

CFLAGS+=	-I${LLDB_SRCS}/include

SRCDIR=		tools/lldb/tools/driver
SRCS+=		Driver.cpp
SRCS+=		Platform.cpp

.include "${.CURDIR}/../../../lib/clang/clang.build.mk"

LIBDEPS+=	lldb
LIBDEPS+=	clang
LIBDEPS+=	llvm

.for lib in ${LIBDEPS}
DPADD+=		${.OBJDIR}/../../../lib/clang/lib${lib}/lib${lib}.a
LDADD+=		${.OBJDIR}/../../../lib/clang/lib${lib}/lib${lib}.a
.endfor

LIBADD+=	edit
LIBADD+=	panel
LIBADD+=	ncursesw
LIBADD+=	pthread
LIBADD+=	z

.include <bsd.prog.mk>
