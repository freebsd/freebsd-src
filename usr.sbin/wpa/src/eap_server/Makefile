.include <src.opts.mk>

.include "../../Makefile.inc"

.include "../../Makefile.crypto"

LIB=	wpaeap_server
INTERNALLIB=

.PATH:	${WPA_DISTDIR}/src/eap_server

SRCS=	eap_server.c \
	eap_server_aka.c \
	eap_server_gpsk.c \
	eap_server_gtc.c \
	eap_server_identity.c \
	eap_server_md5.c \
	eap_server_methods.c \
	eap_server_mschapv2.c \
	eap_server_pax.c \
	eap_server_peap.c \
	eap_server_pwd.c \
	eap_server_sake.c \
	eap_server_sim.c \
	eap_server_tls.c \
	eap_server_tls_common.c \
	eap_server_ttls.c \
	eap_server_wsc.c \
	eap_sim_db.c

# We are only interested in includes at this point. Not libraries.
LIBADD=

.include <bsd.lib.mk>
