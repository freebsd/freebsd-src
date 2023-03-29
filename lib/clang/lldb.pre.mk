# $FreeBSD$

.include "clang.pre.mk"

LLDB_SRCS=		${LLVM_BASE}/lldb

LLDB_TBLGEN?=		lldb-tblgen
LLDB_TBLGEN_BIN!=	which ${LLDB_TBLGEN} || echo __nonexistent_lldb_tblgen__
LLDB_TBLGEN_OPTS?=	--write-if-changed
