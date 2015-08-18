# $FreeBSD$

.PATH: ${.CURDIR}/../../crypto/aesni

KMOD=	aesni
SRCS=	aesni.c
SRCS+=	aeskeys_${MACHINE_CPUARCH}.S
SRCS+=	device_if.h bus_if.h opt_bus.h cryptodev_if.h

OBJS+=	aesni_ghash.o aesni_wrap.o

# Remove -nostdinc so we can get the intrinsics.
aesni_ghash.o: aesni_ghash.c
	# XXX - gcc won't understand -mpclmul
	${CC} -c ${CFLAGS:C/^-O2$/-O3/:N-nostdinc} ${WERROR} ${PROF} \
	     -mmmx -msse -msse4 -maes -mpclmul ${.IMPSRC}
	${CTFCONVERT_CMD}

aesni_wrap.o: aesni_wrap.c
	${CC} -c ${CFLAGS:C/^-O2$/-O3/:N-nostdinc} ${WERROR} ${PROF} \
	     -mmmx -msse -msse4 -maes ${.IMPSRC}
	${CTFCONVERT_CMD}

aesni_ghash.o: aesni.h
aesni_wrap.o: aesni.h

.include <bsd.kmod.mk>

CWARNFLAGS.aesni_ghash.c=	${NO_WCAST_QUAL}
CWARNFLAGS.aesni_wrap.c=	${NO_WCAST_QUAL}
