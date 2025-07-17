.include <src.opts.mk>

.include "../../Makefile.inc"

.include "../../Makefile.crypto"

LIB=	wpaeap_peer
INTERNALLIB=

.PATH:	${WPA_DISTDIR}/src/eap_peer

SRCS=	eap.c \
	eap_gtc.c \
	eap_leap.c \
	eap_md5.c \
	eap_methods.c \
	eap_mschapv2.c \
	eap_otp.c \
	eap_peap.c \
	eap_psk.c \
	eap_tls.c \
	eap_tls_common.c \
	eap_ttls.c \
	eap_wsc.c \
	eap_methods.c \
	mschapv2.c

CFLAGS+=-DIEEE8021X_EAPOL

# We are only interested in includes at this point. Not libraries.
LIBADD=

.include <bsd.lib.mk>
