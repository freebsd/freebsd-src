# $FreeBSD$

.PATH: ${SRCTOP}/sys/contrib/openzfs/module/icp

PACKAGE=	utilities
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
        asm-x86_64/sha2/sha256_impl.S \
        asm-x86_64/sha2/sha512_impl.S

CFLAGS+= -D__amd64 -D_SYS_STACK_H -UHAVE_AES
.else
ASM_SOURCES_C =
ASM_SOURCES_AS =
.endif


KERNEL_C = \
        spi/kcf_spi.c \
        api/kcf_ctxops.c \
        api/kcf_digest.c \
        api/kcf_cipher.c \
        api/kcf_miscapi.c \
        api/kcf_mac.c \
        algs/aes/aes_impl_aesni.c \
        algs/aes/aes_impl_generic.c \
        algs/aes/aes_impl_x86-64.c \
        algs/aes/aes_impl.c \
        algs/aes/aes_modes.c \
        algs/edonr/edonr.c \
        algs/modes/modes.c \
        algs/modes/cbc.c \
        algs/modes/gcm_generic.c \
        algs/modes/gcm_pclmulqdq.c \
        algs/modes/gcm.c \
        algs/modes/ctr.c \
        algs/modes/ccm.c \
        algs/modes/ecb.c \
        algs/sha2/sha2.c \
        algs/skein/skein.c \
        algs/skein/skein_block.c \
        algs/skein/skein_iv.c \
        illumos-crypto.c \
        io/aes.c \
        io/sha2_mod.c \
        io/skein_mod.c \
        os/modhash.c \
        core/kcf_sched.c \
        core/kcf_prov_lib.c \
        core/kcf_callprov.c \
        core/kcf_mech_tabs.c \
        core/kcf_prov_tabs.c \
        $(ASM_SOURCES_C)






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
CFLAGS.sha256_impl.S+= -DLOCORE
CFLAGS.sha512_impl.S+= -DLOCORE

.include <bsd.lib.mk>
