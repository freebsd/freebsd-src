# $FreeBSD$

.PATH:  ${.CURDIR}/../../net

KMOD=   if_ef
SRCS=   if_ef.c opt_ipx.h opt_inet.h
MAN4=	ef.4

# If you need only limited number of frames comment out unneeded ones
# this will reduce number of visible devices
ETHER_II=
ETHER_8023=
ETHER_8022=
ETHER_SNAP=

NBPF?=	1

CFLAGS+= ${PROTOS}

.if defined(ETHER_II)
CFLAGS+= -DETHER_II
.endif

.if defined(ETHER_8023)
CFLAGS+= -DETHER_8023
.endif

.if defined(ETHER_8022)
CFLAGS+= -DETHER_8022
.endif

.if defined(ETHER_SNAP)
CFLAGS+= -DETHER_SNAP
.endif

.if defined(EFDEBUG)
CFLAGS+= -DEF_DEBUG
.endif

opt_inet.h:
	echo "#define INET 1" > opt_inet.h

opt_ipx.h:
	echo "#define IPX 1" > opt_ipx.h

load: all
	/sbin/kldload ./${PROG}

unload:
	/sbin/kldunload ${PROG}

.include <bsd.kmod.mk>
