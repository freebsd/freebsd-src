# $FreeBSD$

.PATH:  ${.CURDIR}/../../net

KMOD=   if_ef
SRCS=   if_ef.c opt_ipx.h opt_inet.h opt_ef.h
NOMAN=

# If you need only limited number of frames comment out unneeded ones
# this will reduce number of visible devices
ETHER_II=
ETHER_8023=
ETHER_8022=
ETHER_SNAP=

.if defined(EFDEBUG)
CFLAGS+= -DEF_DEBUG
.endif

opt_inet.h:
	echo "#define INET 1" > opt_inet.h

opt_ipx.h:
	echo "#define IPX 1" > opt_ipx.h

opt_ef.h:
.for frame in ETHER_II ETHER_8023 ETHER_8022 ETHER_SNAP
.if defined(${frame})
	echo "#define ${frame} 1" >> opt_ef.h
.endif
.endfor

load: all
	/sbin/kldload ./${PROG}

unload:
	/sbin/kldunload ${PROG}

.include <bsd.kmod.mk>
