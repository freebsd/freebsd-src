
.PATH: ${SRCTOP}/sys/contrib/openzfs/module/icp

PACKAGE=	zfs
LIB=	icp
LIBADD=

.if ${MACHINE_ARCH} == "amd64"
ASM_SOURCES_C = asm-x86_64/aes/aeskey.c
ASM_SOURCES_AS = \
        asm-x86_64/aes/aes_amd64.S \
        asm-x86_64/aes/aes_aesni.S \
        asm-x86_64/modes/gcm_pclmulqdq.S \
        asm-x86_64/modes/aesni-gcm-x86_64.S \
        asm-x86_64/modes/ghash-x86_64.S \
        asm-x86_64/sha2/sha256-x86_64.S \
        asm-x86_64/sha2/sha512-x86_64.S \
        asm-x86_64/blake3/blake3_avx2.S \
        asm-x86_64/blake3/blake3_avx512.S \
        asm-x86_64/blake3/blake3_sse2.S \
        asm-x86_64/blake3/blake3_sse41.S

CFLAGS+= -D__amd64 -D_SYS_STACK_H -UHAVE_AES
.elif ${MACHINE_ARCH} == "armv7"
ASM_SOURCES_C =
ASM_SOURCES_AS = \
        asm-arm/sha2/sha256-armv7.S \
        asm-arm/sha2/sha512-armv7.S
.elif ${MACHINE_ARCH} == "aarch64"
ASM_SOURCES_C =
ASM_SOURCES_AS = \
        asm-aarch64/blake3/b3_aarch64_sse2.S \
        asm-aarch64/blake3/b3_aarch64_sse41.S \
        asm-aarch64/sha2/sha256-armv8.S \
        asm-aarch64/sha2/sha512-armv8.S
.elif ${MACHINE_ARCH} == "powerpc64"
ASM_SOURCES_C =
ASM_SOURCES_AS = \
        asm-ppc64/sha2/sha256-ppc.S \
        asm-ppc64/sha2/sha512-ppc.S \
        asm-ppc64/sha2/sha256-p8.S \
        asm-ppc64/sha2/sha512-p8.S
.elif ${MACHINE_ARCH} == "powerpc64le"
ASM_SOURCES_C =
ASM_SOURCES_AS = \
        asm-ppc64/blake3/b3_ppc64le_sse2.S \
        asm-ppc64/blake3/b3_ppc64le_sse41.S \
        asm-ppc64/sha2/sha256-ppc.S \
        asm-ppc64/sha2/sha512-ppc.S \
        asm-ppc64/sha2/sha256-p8.S \
        asm-ppc64/sha2/sha512-p8.S
.else
ASM_SOURCES_C =
ASM_SOURCES_AS =
.endif

KERNEL_C = \
        spi/kcf_spi.c \
        api/kcf_ctxops.c \
        api/kcf_cipher.c \
        api/kcf_mac.c \
        algs/aes/aes_impl_aesni.c \
        algs/aes/aes_impl_generic.c \
        algs/aes/aes_impl_x86-64.c \
        algs/aes/aes_impl.c \
        algs/aes/aes_modes.c \
        algs/blake3/blake3.c \
        algs/blake3/blake3_generic.c \
        algs/blake3/blake3_impl.c \
        algs/edonr/edonr.c \
        algs/modes/modes.c \
        algs/modes/cbc.c \
        algs/modes/gcm_generic.c \
        algs/modes/gcm_pclmulqdq.c \
        algs/modes/gcm.c \
        algs/modes/ctr.c \
        algs/modes/ccm.c \
        algs/modes/ecb.c \
        algs/sha2/sha2_generic.c \
        algs/sha2/sha256_impl.c \
        algs/sha2/sha512_impl.c \
        algs/skein/skein.c \
        algs/skein/skein_block.c \
        algs/skein/skein_iv.c \
        illumos-crypto.c \
        io/aes.c \
        io/sha2_mod.c \
        io/skein_mod.c \
        core/kcf_sched.c \
        core/kcf_prov_lib.c \
        core/kcf_callprov.c \
        core/kcf_mech_tabs.c \
        core/kcf_prov_tabs.c \
        $(ASM_SOURCES_C)

.PATH: ${SRCTOP}/sys/contrib/openzfs/module/zfs
KERNEL_C+=	zfs_impl.c

SRCS= $(ASM_SOURCES_AS) $(KERNEL_C)

WARNS?=	2
SHLIB_MAJOR= 3
CSTD=	c99
CFLAGS+= -DIN_BASE
CFLAGS+= -I${SRCTOP}/sys/contrib/openzfs/include
CFLAGS+= -I${SRCTOP}/sys/contrib/openzfs/lib/libspl/include/
CFLAGS+= -I${SRCTOP}/sys/contrib/openzfs/lib/libspl/include/os/freebsd
CFLAGS+= -I${SRCTOP}/sys
CFLAGS+= -I${SRCTOP}/cddl/compat/opensolaris/include
CFLAGS+= -I${SRCTOP}/sys/contrib/openzfs/module/icp/include
CFLAGS+= -include ${SRCTOP}/sys/contrib/openzfs/include/os/freebsd/spl/sys/ccompile.h
CFLAGS+= -DHAVE_ISSETUGID
CFLAGS+= -include ${SRCTOP}/sys/modules/zfs/zfs_config.h

CFLAGS.aes_amd64.S+= -DLOCORE
CFLAGS.aes_aesni.S+= -DLOCORE
CFLAGS.gcm_pclmulqdq.S+= -DLOCORE
CFLAGS.aesni-gcm-x86_64.S+= -DLOCORE
CFLAGS.ghash-x86_64.S+= -DLOCORE
CFLAGS.sha256-x86_64.S+= -DLOCORE
CFLAGS.sha512-x86_64.S+= -DLOCORE
CFLAGS.blake3_avx2.S = -DLOCORE
CFLAGS.blake3_avx512.S = -DLOCORE
CFLAGS.blake3_sse2.S = -DLOCORE
CFLAGS.blake3_sse41.S = -DLOCORE
CFLAGS.b3_aarch64_sse2.S = -DLOCORE
CFLAGS.b3_aarch64_sse41.S = -DLOCORE
CFLAGS.sha256-armv7.S = -DLOCORE
CFLAGS.sha256-armv8.S = -DLOCORE
CFLAGS.sha512-armv7.S = -DLOCORE
CFLAGS.sha512-armv8.S = -DLOCORE
CFLAGS.b3_ppc64le_sse2.S = -DLOCORE
CFLAGS.b3_ppc64le_sse41.S = -DLOCORE
CFLAGS.sha256-p8.S = -DLOCORE
CFLAGS.sha256-ppc.S = -DLOCORE
CFLAGS.sha512-p8.S = -DLOCORE
CFLAGS.sha512-ppc.S = -DLOCORE


LDFLAGS.bfd+= -Wl,-znoexecstack

.include <bsd.lib.mk>
