PROG_CXX=	clang-tblgen
MAN=

SRCDIR=		clang/utils/TableGen
SRCS+=		ASTTableGen.cpp
SRCS+=		ClangASTNodesEmitter.cpp
SRCS+=		ClangASTPropertiesEmitter.cpp
SRCS+=		ClangAttrEmitter.cpp
SRCS+=		ClangBuiltinsEmitter.cpp
SRCS+=		ClangCommentCommandInfoEmitter.cpp
SRCS+=		ClangCommentHTMLNamedCharacterReferenceEmitter.cpp
SRCS+=		ClangCommentHTMLTagsEmitter.cpp
SRCS+=		ClangDataCollectorsEmitter.cpp
SRCS+=		ClangDiagnosticsEmitter.cpp
SRCS+=		ClangOpcodesEmitter.cpp
SRCS+=		ClangOpenCLBuiltinEmitter.cpp
SRCS+=		ClangOptionDocEmitter.cpp
SRCS+=		ClangSACheckersEmitter.cpp
SRCS+=		ClangSyntaxEmitter.cpp
SRCS+=		ClangTypeNodesEmitter.cpp
SRCS+=		MveEmitter.cpp
SRCS+=		NeonEmitter.cpp
SRCS+=		RISCVVEmitter.cpp
SRCS+=		SveEmitter.cpp
SRCS+=		TableGen.cpp

.include "${SRCTOP}/lib/clang/clang.pre.mk"

CFLAGS.RISCVVEmitter.cpp+=	-I${CLANG_SRCS}/include

.include "../clang.prog.mk"
