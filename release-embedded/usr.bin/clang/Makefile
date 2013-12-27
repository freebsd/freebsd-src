# $FreeBSD$

.include <bsd.own.mk>

SUBDIR=	clang clang-tblgen tblgen

.if !defined(TOOLS_PREFIX)
.if ${MK_CLANG_EXTRAS} != "no"
SUBDIR+=bugpoint \
	llc \
	lli \
	llvm-ar \
	llvm-as \
	llvm-bcanalyzer \
	llvm-diff \
	llvm-dis \
	llvm-extract \
	llvm-link \
	llvm-mc \
	llvm-nm \
	llvm-objdump \
	llvm-prof \
	llvm-ranlib \
	llvm-rtdyld \
	macho-dump \
	opt
.endif

.if ${MK_LLDB} != "no"
SUBDIR+=lldb
.endif
.endif # TOOLS_PREFIX

.include <bsd.subdir.mk>
