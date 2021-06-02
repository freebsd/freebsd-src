.include <src.opts.mk>

.include "../../Makefile.inc"

.include "../../Makefile.crypto"

LIB=	wpaeap_common
INTERNALLIB=

.PATH:	${WPA_DISTDIR}/src/eap_common

SRCS=	chap.c \
	eap_common.c \
	eap_gpsk_common.c \
	eap_pax_common.c \
	eap_peap_common.c \
	eap_psk_common.c \
	eap_sake_common.c \
	eap_sim_common.c \
	eap_wsc_common.c

# We are only interested in includes at this point. Not libraries.
LIBADD=

.include <bsd.lib.mk>
