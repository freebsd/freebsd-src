# $FreeBSD$

MAN=
.include <bsd.own.mk>

PROG_CXX=clang-tblgen

SRCDIR=	tools/clang/utils/TableGen
SRCS=	ClangASTNodesEmitter.cpp \
	ClangAttrEmitter.cpp \
	ClangCommentCommandInfoEmitter.cpp \
	ClangCommentHTMLNamedCharacterReferenceEmitter.cpp \
	ClangCommentHTMLTagsEmitter.cpp \
	ClangDiagnosticsEmitter.cpp \
	ClangSACheckersEmitter.cpp \
	NeonEmitter.cpp \
	TableGen.cpp

LIBDEPS=llvmtablegen \
	llvmsupport

NO_PIE=	yes

.include "../clang.prog.mk"
