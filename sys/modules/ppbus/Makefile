
# $FreeBSD: src/sys/modules/ppbus/Makefile,v 1.3 2002/01/11 15:48:56 ru Exp $

.PATH:	${.CURDIR}/../../dev/ppbus
KMOD=	ppbus
SRCS=	bus_if.h device_if.h ppbus_if.h \
	opt_ppb_1284.h opt_ppbus.h \
	ppb_1284.c ppb_base.c ppb_msq.c ppbconf.c

EXPORT_SYMS=	ppb_attach_device	\
		ppb_request_bus		\
		ppb_release_bus		\
		ppb_get_status		\
		ppb_poll_bus		\
		ppb_reset_epp_timeout	\
		ppb_ecp_sync		\
		ppb_get_epp_protocol	\
		ppb_set_mode		\
		ppb_get_mode		\
		ppb_write

.include <bsd.kmod.mk>
