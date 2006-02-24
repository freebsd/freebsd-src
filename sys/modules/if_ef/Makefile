# $FreeBSD$

.PATH: ${.CURDIR}/../../net

KMOD=	if_ef
SRCS=	if_ef.c opt_ipx.h opt_inet.h opt_ef.h

.if defined(EFDEBUG)
CFLAGS+= -DEF_DEBUG
.endif

.if !defined(KERNBUILDDIR)
# If you need only a limited number of frames, uncomment the needed ones.
# This will reduce the number of visible devices.  Note that the module
# will support all frames by default, i.e., when all are commented here.
#ETHER_II=
#ETHER_8023=
#ETHER_8022=
#ETHER_SNAP=

opt_inet.h:
	echo "#define INET 1" > opt_inet.h

opt_ipx.h:
	echo "#define IPX 1" > opt_ipx.h

opt_ef.h:
	touch opt_ef.h
.for frame in ETHER_II ETHER_8023 ETHER_8022 ETHER_SNAP
.if defined(${frame})
	echo "#define ${frame} 1" >> opt_ef.h
.endif
.endfor
.endif

.include <bsd.kmod.mk>
