# $FreeBSD$
.include <src.opts.mk>

SUBDIR= 	uboot

#
# The BERI boot loader port works only on 64-bit MIPS; not a hard port to
# 32-bit if someone is interested.  Build on all 64-bit MIPS platforms to
# ensure it gets adequate build-test coverage.
#
.if ${MACHINE_ARCH} == "mips64" 
.if ${COMPILER_TYPE} == "gcc"
SUBDIR+=	beri
.endif
.endif

.include <bsd.subdir.mk>
