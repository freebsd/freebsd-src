# $FreeBSD$

.include <bsd.own.mk>

LIB=	clangformat

SRCDIR=	tools/clang/lib/Format
SRCS=	AffectedRangeManager.cpp \
	BreakableToken.cpp \
	ContinuationIndenter.cpp \
	Format.cpp \
	FormatToken.cpp \
	FormatTokenLexer.cpp \
	SortJavaScriptImports.cpp \
	TokenAnalyzer.cpp \
	TokenAnnotator.cpp \
	UnwrappedLineFormatter.cpp \
	UnwrappedLineParser.cpp \
	WhitespaceManager.cpp

TGHDRS=	DiagnosticCommonKinds

.include "../clang.lib.mk"
