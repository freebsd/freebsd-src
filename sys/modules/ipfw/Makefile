# $FreeBSD$

.PATH: ${.CURDIR}/../../netinet

KMOD=	ipfw
SRCS=	ip_fw2.c ip_fw_pfil.c
SRCS+=	opt_inet6.h opt_ipsec.h

CFLAGS+= -DIPFIREWALL
#
#If you want it verbose
#CFLAGS+= -DIPFIREWALL_VERBOSE
#CFLAGS+= -DIPFIREWALL_VERBOSE_LIMIT=100
#
#If you want it to pass all packets by default
#CFLAGS+= -DIPFIREWALL_DEFAULT_TO_ACCEPT
#

.if !defined(KERNBUILDDIR)
.if !defined(NO_INET6)
opt_inet6.h:
	echo "#define INET6 1" > ${.TARGET}
.endif
.endif

.include <bsd.kmod.mk>
