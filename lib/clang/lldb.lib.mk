# $FreeBSD$

LLDB_SRCS= ${.CURDIR}/../../../contrib/llvm/tools/lldb

CFLAGS+=-I${LLDB_SRCS}/include -I${LLDB_SRCS}/source
CXXFLAGS+=-std=c++11 -DLLDB_DISABLE_PYTHON                      

.include "clang.lib.mk"
