# $FreeBSD$

# FreeBSD didn't always have the __FBSDID() macro in <sys/cdefs.h>.
# We could do this with a sys/cdefs.h wrapper, but given that this would
# slow down all new builds for such a simple concept, we do it here.
.if defined(BOOTSTRAPPING) && \
    ( ${BOOTSTRAPPING} < 440001 || \
    ( ${BOOTSTRAPPING} >= 500000 && ${BOOTSTRAPPING} < 500024 ))
CFLAGS+=	-D__FBSDID=__RCSID
.endif

CFLAGS+=	-I${WORLDTMP}/build/usr/include
DPADD=		${WORLDTMP}/build/usr/lib/libbuild.a
LDADD=		-lbuild
LDFLAGS=	-L${WORLDTMP}/build/usr/lib

OLD_MAKE_CONF?=	/etc/make.conf
.if exists(${OLD_MAKE_CONF})
.include "${OLD_MAKE_CONF}"
.endif
