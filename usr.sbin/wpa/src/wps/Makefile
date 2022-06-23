.include <src.opts.mk>

.include "../../Makefile.inc"

.include "../../Makefile.crypto"

LIB=	wpawps
INTERNALLIB=

.PATH:	${WPA_DISTDIR}/src/wps

SRCS=	http_client.c \
	httpread.c \
	http_server.c \
	upnp_xml.c \
	wps_attr_build.c \
	wps_attr_parse.c \
	wps_attr_process.c \
	wps.c \
	wps_common.c \
	wps_dev_attr.c \
	wps_enrollee.c \
	wps_registrar.c \
	wps_upnp_ap.c \
	wps_upnp.c \
	wps_upnp_event.c \
	wps_upnp_ssdp.c \
	wps_upnp_web.c

.if ${MK_INET6} != "no"
CFLAGS+= -DCONFIG_IPV6
.endif

# We are only interested in includes at this point. Not libraries.
LIBADD=

.include <bsd.lib.mk>
