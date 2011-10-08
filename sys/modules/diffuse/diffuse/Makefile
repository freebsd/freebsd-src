# $FreeBSD$

.include <bsd.own.mk>

.PATH:	${.CURDIR}/../../../netinet/ipfw

KMOD=	diffuse
SRCS=	diffuse_export.c \
	diffuse_flowtable.c \
	ip_diffuse.c \
	opt_inet6.h

.if !defined(KERNBUILDDIR)
.if ${MK_INET_SUPPORT} != "no"
opt_inet.h:
	echo "#define INET 1" > ${.TARGET}
.endif
.if ${MK_INET6_SUPPORT} != "no"
opt_inet6.h:
	echo "#define INET6 1" > ${.TARGET}
.endif
.endif

.include <bsd.kmod.mk>
