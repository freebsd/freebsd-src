# $FreeBSD$

.include <src.opts.mk>

LIB=	llvmobjectyaml

SRCDIR=	lib/ObjectYAML
SRCS=	COFFYAML.cpp \
	ELFYAML.cpp \
	MachOYAML.cpp

.if ${MK_CLANG_EXTRAS} != "no"
SRCS+=	ObjectYAML.cpp \
	YAML.cpp
.endif

.include "../clang.lib.mk"
