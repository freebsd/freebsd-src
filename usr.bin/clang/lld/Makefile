# $FreeBSD$

.include <src.opts.mk>

LLVM_SRCS=	${SRCTOP}/contrib/llvm
LLD_SRCS=	${LLVM_SRCS}/tools/lld

PACKAGE=	lld
PROG_CXX=	ld.lld
MAN=
.if ${MK_LLD_IS_LD} != "no"
SYMLINKS=	${PROG_CXX} ${BINDIR}/ld
.endif

CFLAGS+=	-I${LLD_SRCS}/include
CFLAGS+=	-I${.OBJDIR}
CFLAGS+=	-I${OBJTOP}/lib/clang/libllvm

SRCDIR=		tools/lld
SRCS+=		ELF/Driver.cpp
SRCS+=		ELF/DriverUtils.cpp
SRCS+=		ELF/EhFrame.cpp
SRCS+=		ELF/Error.cpp
SRCS+=		ELF/Filesystem.cpp
SRCS+=		ELF/GdbIndex.cpp
SRCS+=		ELF/ICF.cpp
SRCS+=		ELF/InputFiles.cpp
SRCS+=		ELF/InputSection.cpp
SRCS+=		ELF/LTO.cpp
SRCS+=		ELF/LinkerScript.cpp
SRCS+=		ELF/MapFile.cpp
SRCS+=		ELF/MarkLive.cpp
SRCS+=		ELF/Mips.cpp
SRCS+=		ELF/OutputSections.cpp
SRCS+=		ELF/Relocations.cpp
SRCS+=		ELF/ScriptLexer.cpp
SRCS+=		ELF/ScriptParser.cpp
SRCS+=		ELF/Strings.cpp
SRCS+=		ELF/SymbolTable.cpp
SRCS+=		ELF/Symbols.cpp
SRCS+=		ELF/SyntheticSections.cpp
SRCS+=		ELF/Target.cpp
SRCS+=		ELF/Thunks.cpp
SRCS+=		ELF/Writer.cpp
SRCS+=		lib/Config/Version.cpp
SRCS+=		lib/Core/Error.cpp
SRCS+=		lib/Core/File.cpp
SRCS+=		lib/Core/LinkingContext.cpp
SRCS+=		lib/Core/Reader.cpp
SRCS+=		lib/Core/Reproduce.cpp
SRCS+=		lib/Core/Resolver.cpp
SRCS+=		lib/Core/SymbolTable.cpp
SRCS+=		lib/Core/TargetOptionsCommandFlags.cpp
SRCS+=		tools/lld/lld.cpp

.include "${SRCTOP}/lib/clang/llvm.build.mk"

LIBDEPS+=	llvm

.for lib in ${LIBDEPS}
DPADD+=		${OBJTOP}/lib/clang/lib${lib}/lib${lib}.a
LDADD+=		${OBJTOP}/lib/clang/lib${lib}/lib${lib}.a
.endfor

LLVM_TBLGEN?=	llvm-tblgen
ELF/Options.inc: ${LLD_SRCS}/ELF/Options.td
	${LLVM_TBLGEN} -gen-opt-parser-defs \
	    -I ${LLVM_SRCS}/include \
	    -d ${.TARGET:C/$/.d/} -o ${.TARGET} \
	    ${LLVM_SRCS}/tools/lld/ELF/Options.td
TGHDRS+=	ELF/Options.inc

DPSRCS+=	${TGHDRS}
CLEANFILES+=	${TGHDRS} ${TGHDRS:C/$/.d/}

LIBADD+=	ncursesw
LIBADD+=	pthread
LIBADD+=	z

.include <bsd.prog.mk>
