.PATH:		${SRCTOP}/sys/dev/smbus
KMOD		= smbus
SRCS		= device_if.h bus_if.h smbus_if.h smbus_if.c \
		  smbconf.h smbconf.c smbus.h smbus.c

EXPORT_SYMS=	YES

.include <bsd.kmod.mk>
