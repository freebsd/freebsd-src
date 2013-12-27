# $FreeBSD$

.include "${.CURDIR}/../Makefile.inc"

.PATH.c:${WPA_SUPPLICANT_DISTDIR} \
	${WPA_DISTDIR}/src/drivers

PROG=	wpa_priv
SRCS=	drivers.c os_unix.c eloop.c common.c wpa_debug.c wpabuf.c wpa_priv.c \
	driver_common.c l2_packet_freebsd.c

DPADD+=	${LIBPCAP}
LDADD+=	-lpcap

.include "${.CURDIR}/../Makefile.crypto"

.include <bsd.prog.mk>
