# $FreeBSD$

.PATH: ${.CURDIR}/../../net ${.CURDIR}/../../netinet ${.CURDIR}/../../netinet6

KMOD=	if_gif
SRCS=	if_gif.c in_gif.c opt_inet.h opt_inet6.h opt_mac.h \
	opt_mrouting.h
.if !defined(NO_INET6)
SRCS+=	in6_gif.c
.endif

# XXX The new way of building modules with the kernel can't handle
#     dynamic SRCS depending on build options yet.
##.if !defined(KERNBUILDDIR)
opt_inet.h:
	echo "#define INET 1" > ${.TARGET}

.if !defined(NO_INET6)
opt_inet6.h:
	echo "#define INET6 1" > ${.TARGET}
.endif

opt_mrouting.h:
	echo "#define MROUTING 1" > ${.TARGET}
##.endif

.include <bsd.kmod.mk>
