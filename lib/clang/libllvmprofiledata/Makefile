# $FreeBSD$

.include <src.opts.mk>

LIB=	llvmprofiledata

SRCDIR=	lib/ProfileData
SRCS=	CoverageMapping.cpp \
	CoverageMappingReader.cpp \
	CoverageMappingWriter.cpp \
	InstrProf.cpp \
	InstrProfReader.cpp \
	InstrProfWriter.cpp \
	SampleProf.cpp \
	SampleProfReader.cpp

.if ${MK_CLANG_EXTRAS} != "no"
SRCS+=	SampleProfWriter.cpp
.endif

TGHDRS=	Attributes

.include "../clang.lib.mk"
