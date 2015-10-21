# $FreeBSD$

.include <src.opts.mk>

SUBDIR=	clang clang-tblgen tblgen

.if !defined(TOOLS_PREFIX)
.if ${MK_CLANG_EXTRAS} != "no"
SUBDIR+=bugpoint \
	llc \
	lli \
	llvm-ar \
	llvm-as \
	llvm-bcanalyzer \
	llvm-cov \
	llvm-cxxdump \
	llvm-diff \
	llvm-dis \
	llvm-dwarfdump \
	llvm-extract \
	llvm-link \
	llvm-lto \
	llvm-mc \
	llvm-nm \
	llvm-objdump \
	llvm-pdbdump \
	llvm-profdata \
	llvm-rtdyld \
	llvm-symbolizer \
	macho-dump \
	opt
.endif

.if ${MK_LLDB} != "no"
SUBDIR+=lldb
.endif
.endif # TOOLS_PREFIX

SUBDIR_PARALLEL=

.include <bsd.subdir.mk>
