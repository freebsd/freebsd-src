#	Makefile,v 1.3 1995/05/30 06:06:07 rgrimes Exp

.PATH:	${.CURDIR}/../../sys/netinet
KMOD=	ipfw_mod
SRCS=	ipfw_lkm.c ip_fw.c
NOMAN=
CFLAGS+= -DIPFIREWALL  -DIPACCT
#
#If you want it verbose
#CFLAGS+= -DIPFIREWALL_VERBOSE
#

.include <bsd.kmod.mk>
