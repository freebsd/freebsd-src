.include <src.opts.mk>

.include "../../Makefile.inc"

.include "../../Makefile.crypto"

LIB=	wpacommon
INTERNALLIB=

.PATH:	${WPA_DISTDIR}/src/common

SRCS=	cli.c \
	ctrl_iface_common.c \
	dragonfly.c \
	dpp_tcp.c \
	gas.c \
	hw_features_common.c \
	ieee802_11_common.c \
	ptksa_cache.c \
	wpa_common.c \
	wpa_ctrl.c

CFLAGS+=-DCONFIG_SAE \
	-DCONFIG_SUITE \
	-DCONFIG_SUITEB

# We are only interested in includes at this point. Not libraries.
LIBADD=

.include <bsd.lib.mk>
