# $FreeBSD$

.include <src.opts.mk>

.include "../Makefile.inc"

.include "../Makefile.crypto"

.PATH.c:${WPA_SUPPLICANT_DISTDIR}

PACKAGE=	wpa
PROG=	wpa_cli
SRCS=	wpa_cli.c

MAN=	wpa_cli.8

CFLAGS+= -DCONFIG_TLS=openssl
# enable use of d_type to identify unix domain sockets
CFLAGS+= -D_DIRENT_HAVE_D_TYPE

CFLAGS+= -DCONFIG_WPA_CLI_EDIT=y
LIBADD+=wpaap \
	wpacommon \
	wpacrypto \
	wpaeap_common \
	wpaeap_peer \
	wpaeap_server \
	wpaeapol_auth \
	wpaeapol_supp \
	wpal2_packet \
	wparadius \
	wparsn_supp \
	wpatls \
	wpautils
LIBADD+=	pcap util

.include <bsd.prog.mk>
