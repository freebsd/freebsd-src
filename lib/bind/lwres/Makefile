# $FreeBSD$

BIND_DIR=	${.CURDIR}/../../../contrib/bind9
LIB_BIND_REL=	..
LIB_BIND_DIR=	${.CURDIR}/${LIB_BIND_REL}
SRCDIR=		${BIND_DIR}/lib/lwres

# Unlike other BIND libs, this should be installed unless the user says NO.
.if !defined(NO_BIND_LIBS_LWRES)
WITH_BIND_LIBS=
.endif

.include	"${LIB_BIND_DIR}/config.mk"

LIB=		lwres

.PATH:		${SRCDIR} ${SRCDIR}/man
SRCS+=		context.c gai_strerror.c getaddrinfo.c gethost.c \
		getipnode.c getnameinfo.c getrrset.c herror.c \
		lwbuffer.c lwconfig.c lwpacket.c lwresutil.c \
		lwres_gabn.c lwres_gnba.c lwres_grbn.c lwres_noop.c \
		lwinetaton.c lwinetpton.c lwinetntop.c print.c

CFLAGS+=	-I${SRCDIR}/unix/include -I${SRCDIR}/include
CFLAGS+=	-I${.CURDIR}

.if defined(WITH_BIND_LIBS)
MAN=		lwres.3 lwres_buffer.3 lwres_config.3 lwres_context.3 \
		lwres_gabn.3 lwres_gai_strerror.3 lwres_getaddrinfo.3 \
		lwres_gethostent.3 lwres_getipnode.3 lwres_getnameinfo.3 \
		lwres_getrrsetbyname.3 lwres_gnba.3 lwres_hstrerror.3 \
		lwres_inetntop.3 lwres_noop.3 lwres_packet.3 lwres_resutil.3

MLINKS=		lwres_buffer.3 lwres_buffer_add.3 \
		lwres_buffer.3 lwres_buffer_back.3 \
		lwres_buffer.3 lwres_buffer_clear.3 \
		lwres_buffer.3 lwres_buffer_first.3 \
		lwres_buffer.3 lwres_buffer_forward.3 \
		lwres_buffer.3 lwres_buffer_getmem.3 \
		lwres_buffer.3 lwres_buffer_getuint16.3 \
		lwres_buffer.3 lwres_buffer_getuint32.3 \
		lwres_buffer.3 lwres_buffer_getuint8.3 \
		lwres_buffer.3 lwres_buffer_init.3 \
		lwres_buffer.3 lwres_buffer_invalidate.3 \
		lwres_buffer.3 lwres_buffer_putmem.3 \
		lwres_buffer.3 lwres_buffer_putuint16.3 \
		lwres_buffer.3 lwres_buffer_putuint32.3 \
		lwres_buffer.3 lwres_buffer_putuint8.3 \
		lwres_buffer.3 lwres_buffer_subtract.3 \
		lwres_config.3 lwres_conf_clear.3 \
		lwres_config.3 lwres_conf_get.3 \
		lwres_config.3 lwres_conf_init.3 \
		lwres_config.3 lwres_conf_parse.3 \
		lwres_config.3 lwres_conf_print.3 \
		lwres_context.3 lwres_context_allocmem.3 \
		lwres_context.3 lwres_context_create.3 \
		lwres_context.3 lwres_context_destroy.3 \
		lwres_context.3 lwres_context_freemem.3 \
		lwres_context.3 lwres_context_initserial.3 \
		lwres_context.3 lwres_context_nextserial.3 \
		lwres_context.3 lwres_context_sendrecv.3 \
		lwres_gabn.3 lwres_gabnrequest_free.3 \
		lwres_gabn.3 lwres_gabnrequest_parse.3 \
		lwres_gabn.3 lwres_gabnrequest_render.3 \
		lwres_gabn.3 lwres_gabnresponse_free.3 \
		lwres_gabn.3 lwres_gabnresponse_parse.3 \
		lwres_gabn.3 lwres_gabnresponse_render.3 \
		lwres_getaddrinfo.3 lwres_freeaddrinfo.3 \
		lwres_gethostent.3 lwres_endhostent.3 \
		lwres_gethostent.3 lwres_endhostent_r.3 \
		lwres_gethostent.3 lwres_gethostbyaddr.3 \
		lwres_gethostent.3 lwres_gethostbyaddr_r.3 \
		lwres_gethostent.3 lwres_gethostbyname.3 \
		lwres_gethostent.3 lwres_gethostbyname2.3 \
		lwres_gethostent.3 lwres_gethostbyname_r.3 \
		lwres_gethostent.3 lwres_gethostent_r.3 \
		lwres_gethostent.3 lwres_sethostent.3 \
		lwres_gethostent.3 lwres_sethostent_r.3 \
		lwres_getipnode.3 lwres_freehostent.3 \
		lwres_getipnode.3 lwres_getipnodebyaddr.3 \
		lwres_getipnode.3 lwres_getipnodebyname.3 \
		lwres_gnba.3 lwres_gnbarequest_free.3 \
		lwres_gnba.3 lwres_gnbarequest_parse.3 \
		lwres_gnba.3 lwres_gnbarequest_render.3 \
		lwres_gnba.3 lwres_gnbaresponse_free.3 \
		lwres_gnba.3 lwres_gnbaresponse_parse.3 \
		lwres_gnba.3 lwres_gnbaresponse_render.3 \
		lwres_hstrerror.3 lwres_herror.3 \
		lwres_inetntop.3 lwres_net_ntop.3 \
		lwres_noop.3 lwres_nooprequest_free.3 \
		lwres_noop.3 lwres_nooprequest_parse.3 \
		lwres_noop.3 lwres_nooprequest_render.3 \
		lwres_noop.3 lwres_noopresponse_free.3 \
		lwres_noop.3 lwres_noopresponse_parse.3 \
		lwres_noop.3 lwres_noopresponse_render.3 \
		lwres_packet.3 lwres_lwpacket_parseheader.3 \
		lwres_packet.3 lwres_lwpacket_renderheader.3 \
		lwres_resutil.3 lwres_addr_parse.3 \
		lwres_resutil.3 lwres_getaddrsbyname.3 \
		lwres_resutil.3 lwres_getnamebyaddr.3 \
		lwres_resutil.3 lwres_string_parse.3

INCS=		${SRCDIR}/include/lwres/context.h \
		${SRCDIR}/include/lwres/int.h \
		${SRCDIR}/include/lwres/ipv6.h \
		${SRCDIR}/include/lwres/lang.h \
		${SRCDIR}/include/lwres/list.h \
		${SRCDIR}/include/lwres/lwbuffer.h \
		${SRCDIR}/include/lwres/lwpacket.h \
		${SRCDIR}/include/lwres/lwres.h \
		${SRCDIR}/include/lwres/result.h \
		${SRCDIR}/include/lwres/version.h \
		${SRCDIR}/unix/include/lwres/net.h \
		lwres/netdb.h \
		lwres/platform.h

INCSDIR=	${INCLUDEDIR}/lwres
.endif

.include <bsd.lib.mk>
