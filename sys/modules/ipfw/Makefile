# $FreeBSD$

.PATH: ${.CURDIR}/../../netinet

KMOD=	ipfw
SRCS=	ip_fw2.c ip_fw_pfil.c \
	opt_inet.h opt_ipfw.h opt_ipdn.h opt_ipdivert.h opt_ipsec.h \
	opt_pfil_hooks.h

CFLAGS+= -DIPFIREWALL
#
#If you want it verbose
#CFLAGS+= -DIPFIREWALL_VERBOSE
#CFLAGS+= -DIPFIREWALL_VERBOSE_LIMIT=100
#
#If you want it to pass all packets by default
#CFLAGS+= -DIPFIREWALL_DEFAULT_TO_ACCEPT
#
opt_inet.h:
	echo "#define INET 1" > opt_inet.h

.include <bsd.kmod.mk>
