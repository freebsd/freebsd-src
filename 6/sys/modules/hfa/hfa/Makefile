# $FreeBSD$

.PATH:	${.CURDIR}/../../../dev/hfa

KMOD=	hfa
SRCS=	hfa_freebsd.c \
	fore_buffer.c fore_init.c fore_receive.c fore_vcm.c \
	fore_command.c fore_intr.c fore_stats.c \
	fore_globals.c fore_timer.c \
	fore_if.c fore_output.c fore_transmit.c \
	device_if.h bus_if.h

.include <bsd.kmod.mk>
