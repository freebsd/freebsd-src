#$FreeBSD$

.PATH:  ${.CURDIR}/../../../dev/hyperv/netvsc

KMOD    = hv_netvsc

SRCS    = hv_net_vsc.c \
  hv_netvsc_drv_freebsd.c \
	  hv_rndis_filter.c

CFLAGS 	+= -I${.CURDIR}/../../../dev/hyperv/include \
	   -I${.CURDIR}/../../../dev/hyperv/netvsc

.include <bsd.kmod.mk>
