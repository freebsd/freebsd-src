# $FreeBSD$

.include <bsd.own.mk>

.PATH: ${.CURDIR}/../../netpfil/ipfw

KMOD=	ipfw
SRCS=	ip_fw2.c ip_fw_pfil.c
SRCS+=	ip_fw_dynamic.c ip_fw_log.c
SRCS+=	ip_fw_sockopt.c ip_fw_table.c
SRCS+=	opt_inet.h opt_inet6.h opt_ipdivert.h opt_ipfw.h opt_ipsec.h

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
