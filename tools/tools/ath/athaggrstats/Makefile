# $FreeBSD$

.PATH:	${.CURDIR}/../../../../sys/dev/ath/ath_hal

PROG=	athaggrstats

SRCS=	main.c statfoo.c athaggrstats.c opt_ah.h ah_osdep.h

CLEANFILES+=	opt_ah.h

.include <../Makefile.inc>

CFLAGS+=-DATH_SUPPORT_ANI
CFLAGS+=-DATH_SUPPORT_TDMA

opt_ah.h:
	echo "#define AH_DEBUG 1" > opt_ah.h
	echo "#define AH_DEBUG_COUNTRY 1" >> opt_ah.h
	echo "#define AH_SUPPORT_AR5416 1" >> opt_ah.h

ah_osdep.h:
	echo 'typedef void *HAL_SOFTC;' >ah_osdep.h
	echo 'typedef int HAL_BUS_TAG;' >>ah_osdep.h
	echo 'typedef void *HAL_BUS_HANDLE;' >>ah_osdep.h
	echo 'typedef uint32_t HAL_DMA_ADDR;' >>ah_osdep.h

.include <bsd.prog.mk>
