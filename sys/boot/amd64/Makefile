# $FreeBSD$

.include <bsd.own.mk>

# In-tree GCC does not support __attribute__((ms_abi)), required by the
# UEFI loader.
.if ${COMPILER_TYPE} != "gcc"
SUBDIR=		efi
.endif

.include <bsd.subdir.mk>
