# $FreeBSD$

.include <src.opts.mk>

SUBDIR+=	clang
SUBDIR+=	clang-tblgen
SUBDIR+=	llvm-tblgen

.if ${MK_LLDB} != "no"
SUBDIR+=	lldb-tblgen
.endif

.if !defined(TOOLS_PREFIX)
# LLVM binutils are needed to support features such as LTO, so we build them
# by default if clang is enabled. If MK_LLVM_BINUTILS is set, we also use them
# as the default binutils (ar,nm,addr2line, etc.).
.if ${MK_CLANG} != "no" || ${MK_LLVM_BINUTILS} != "no"
SUBDIR+=	llvm-ar
SUBDIR+=	llvm-nm
SUBDIR+=	llvm-objcopy
SUBDIR+=	llvm-objdump
SUBDIR+=	llvm-size
SUBDIR+=	llvm-strings
SUBDIR+=	llvm-symbolizer
.endif

.if ${MK_LLVM_BINUTILS} != "no" || ${MK_LLVM_CXXFILT} != "no"
SUBDIR+=	llvm-cxxfilt
.endif

.if ${MK_CLANG_EXTRAS} != "no"
SUBDIR+=	bugpoint
SUBDIR+=	llc
SUBDIR+=	lli
SUBDIR+=	llvm-as
SUBDIR+=	llvm-bcanalyzer
SUBDIR+=	llvm-cxxdump
SUBDIR+=	llvm-diff
SUBDIR+=	llvm-dis
SUBDIR+=	llvm-dwarfdump
SUBDIR+=	llvm-dwp
SUBDIR+=	llvm-extract
SUBDIR+=	llvm-link
SUBDIR+=	llvm-lto
SUBDIR+=	llvm-lto2
SUBDIR+=	llvm-mc
SUBDIR+=	llvm-mca
SUBDIR+=	llvm-modextract
SUBDIR+=	llvm-pdbutil
SUBDIR+=	llvm-rtdyld
SUBDIR+=	llvm-xray
SUBDIR+=	opt
.endif

.if ${MK_CLANG_EXTRAS} != "no" || ${MK_CLANG_FORMAT} != "no"
SUBDIR+=	clang-format
.endif

.if ${MK_LLD} != "no"
SUBDIR+=	lld
.endif
.if ${MK_LLDB} != "no"
SUBDIR+=	lldb
SUBDIR+=	lldb-server
.endif
.if ${MK_LLVM_COV} != "no"
SUBDIR+=	llvm-cov
SUBDIR+=	llvm-profdata
.endif
.endif # TOOLS_PREFIX

SUBDIR_PARALLEL=

.include <bsd.subdir.mk>
