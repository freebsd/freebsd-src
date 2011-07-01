# $FreeBSD$

LIB=	llvmjit

SRCDIR=	lib/ExecutionEngine/JIT
SRCS=	Intercept.cpp \
	JIT.cpp \
	JITDebugRegisterer.cpp \
	JITDwarfEmitter.cpp \
	JITEmitter.cpp \
	JITMemoryManager.cpp \
	OProfileJITEventListener.cpp

.if ${MACHINE_ARCH} == "ia64"
CFLAGS+= -D__USING_SJLJ_EXCEPTIONS__
.endif

.include "../clang.lib.mk"
