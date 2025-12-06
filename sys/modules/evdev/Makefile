.PATH: ${SRCTOP}/sys/dev/evdev

KMOD=	evdev
SRCS=	cdev.c evdev.c evdev_mt.c evdev_utils.c
SRCS+=	opt_evdev.h opt_kbd.h bus_if.h device_if.h

EXPORT_SYMS=	YES

.include <bsd.kmod.mk>
