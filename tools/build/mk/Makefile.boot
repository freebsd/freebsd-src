# $FreeBSD$

# FreeBSD didn't always have the __FBSDID() macro in <sys/cdefs.h>.
# We could do this with a sys/cdefs.h wrapper, but given that this would
# slow down all new builds for such a simple concept, we do it here.
.if ( ${BOOTSTRAPPING} < 440001 || \
    ( ${BOOTSTRAPPING} >= 500000 && ${BOOTSTRAPPING} < 500024 ))
CFLAGS+=	-D__FBSDID=__RCSID
.endif

CFLAGS+=	-I${WORLDTMP}/legacy/usr/include
DPADD+=		${WORLDTMP}/legacy/usr/lib/libegacy.a
LDADD+=		-legacy
LDFLAGS+=	-L${WORLDTMP}/legacy/usr/lib
