# $FreeBSD$

.include "clang.build.mk"

CXX:=${CXX:C/^c\+\+|^clang\+\+/g++/}

INTERNALLIB=

.include <bsd.lib.mk>
