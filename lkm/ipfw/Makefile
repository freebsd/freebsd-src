#	$Id: Makefile,v 1.1 1994/09/21 23:27:09 wollman Exp $

.PATH:	${.CURDIR}/../../sys/netinet
KMOD=	ipfw_mod
SRCS=	ipfw_lkm.c ip_fw.c 
NOMAN=
CFLAGS+= -DIPFIREWALL  -DIPACCT
#
# If you want it verbose
#CFLAGS+= -DIPFIREWALL_VERBOSE
#

.include <bsd.kmod.mk>
