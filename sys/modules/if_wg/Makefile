# $FreeBSD$


KMOD=   if_wg

INCDIR= ${SRCTOP}/sys/dev/if_wg/include
ZINCDIR= ${SRCTOP}/sys/dev/if_wg/module/crypto/zinc

.PATH: ${SRCTOP}/sys/dev/if_wg/module
.PATH: ${ZINCDIR}
.PATH: ${ZINCDIR}/chacha20
.PATH: ${ZINCDIR}/poly1305

CFLAGS+= -I${INCDIR}

CFLAGS+= -D__KERNEL__

SRCS= opt_inet.h opt_inet6.h device_if.h bus_if.h ifdi_if.h

SRCS+= if_wg_session.c module.c
SRCS+= wg_noise.c wg_cookie.c
SRCS+= curve25519.c blake2s.c
SRCS+= chacha20poly1305.c chacha20.c poly1305.c

.if ${MACHINE_ARCH} == "amd64"
SRCS += poly1305-x86_64.S chacha20-x86_64.S
SIMD_FLAGS = -DCONFIG_AS_SSSE3=1 -DCONFIG_AS_AVX=1 \
	-DCONFIG_AS_AVX512=1 -DCONFIG_AS_AVX2=1
.endif
.include <bsd.kmod.mk>

.if ${MACHINE_ARCH} == "amd64"
CFLAGS.poly1305-x86_64.S = -D__LOCORE -gdwarf-4 ${SIMD_FLAGS}  -include ${INCDIR}/sys/support.h
CFLAGS.chacha20-x86_64.S = -D__LOCORE -gdwarf-4 ${SIMD_FLAGS} -include ${INCDIR}/sys/support.h
CFLAGS.chacha20poly1305.c = -DCONFIG_ZINC_ARCH_X86_64
CFLAGS.chacha20.c = -DCONFIG_ZINC_ARCH_X86_64
CFLAGS.poly1305.c = -DCONFIG_ZINC_ARCH_X86_64
.endif
