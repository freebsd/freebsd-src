# $FreeBSD$

LLVM_SRCS= ${.CURDIR}/../../../contrib/llvm

.include "clang.build.mk"

INTERNALLIB=

.if ${MACHINE_CPUARCH} == "arm"
# This will need to be enabled to link clang 3.8
#STATIC_CXXFLAGS+= -mlong-calls
.endif

.include <bsd.lib.mk>
