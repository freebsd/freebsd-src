# $FreeBSD$

# FreeBSD didn't always have the __FBSDID() macro in <sys/cdefs.h>.
.if defined(BOOTSTRAPPING) && \
    ( ${BOOTSTRAPPING} < 440001 || \
    ( ${BOOTSTRAPPING} >= 500000 && ${BOOTSTRAPPING} < 500024 ))
CFLAGS+=	-D__FBSDID=__RCSID
.endif

CFLAGS+=	-I${WORLDTMP}/usr/include
DPADD=		${WORLDTMP}/usr/lib/libbuild.a
LDADD=		-lbuild
LDFLAGS=	-L${WORLDTMP}/usr/lib

OLD_MAKE_CONF?=	/etc/make.conf
.if exists(${OLD_MAKE_CONF})
.include "${OLD_MAKE_CONF}"
.endif
