.PATH: ${SRCTOP}/sys/dev/wg

KMOD= if_wg

SRCS= if_wg.c wg_cookie.c wg_crypto.c wg_noise.c
SRCS+= opt_inet.h opt_inet6.h device_if.h bus_if.h

.include <bsd.kmod.mk>

CFLAGS+= -include ${SRCTOP}/sys/dev/wg/compat.h
