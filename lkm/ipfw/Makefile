# $FreeBSD$

.PATH:	${.CURDIR}/../../sys/netinet
KMOD=	ipfw_mod
SRCS=	ip_fw.c
NOMAN=
CFLAGS+= -DACTUALLY_LKM_NOT_KERNEL -DIPFIREWALL  -DIPACCT 
#
#If you want it verbose
#CFLAGS+= -DIPFIREWALL_VERBOSE
#

.include <bsd.kmod.mk>
