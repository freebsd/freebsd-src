# $FreeBSD$
#
# Compile netmap as a module, useful if you want a netmap bridge
# or loadable drivers.

.PATH: ${.CURDIR}/../../dev/netmap
.PATH.h: ${.CURDIR}/../../net
CFLAGS += -I${.CURDIR}/../../
KMOD	= netmap
SRCS	= device_if.h bus_if.h opt_netmap.h
SRCS	+= netmap.c netmap.h netmap_kern.h
SRCS	+= netmap_mem2.c netmap_mem2.h
SRCS	+= netmap_generic.c
SRCS	+= netmap_mbq.c netmap_mbq.h
SRCS	+= netmap_vale.c
SRCS	+= netmap_freebsd.c
SRCS	+= netmap_offloadings.c
SRCS	+= netmap_pipe.c
SRCS	+= netmap_monitor.c

.include <bsd.kmod.mk>
