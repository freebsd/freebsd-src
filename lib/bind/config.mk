# $FreeBSD$

CFLAGS+=	-DVERSION='"9.3.0rc4"'

.if defined(SRCDIR) && exists(${SRCDIR}/api)
.include	"${SRCDIR}/api"
.endif

.if defined(LIB_BIND_DIR) && exists(${LIB_BIND_DIR}/config.h)
CFLAGS+=	-I${LIB_BIND_DIR}
.endif

.if defined(LIBINTERFACE)
CFLAGS+=	-DLIBINTERFACE=${LIBINTERFACE}
SHLIB_MAJOR=	${LIBINTERFACE}
.endif

.if defined(LIBREVISION)
CFLAGS+=	-DLIBREVISION=${LIBREVISION}
SHLIB_MINOR=	${LIBINTERFACE}
.endif

.if defined(LIBAGE)
CFLAGS+=	-DLIBAGE=${LIBAGE}
.endif

CFLAGS+=	-DHAVE_CONFIG_H

# GSSAPI support is incomplete in 9.3.0rc4
#.if !defined(NO_KERBEROS)
#CFLAGS+=	-DGSSAPI
#.endif

.if !defined(NOINET6)
CFLAGS+=	-DWANT_IPV6
.endif

.if ${MACHINE_ARCH} == powerpc || ${MACHINE_ARCH} == sparc64
CFLAGS+=	-DWORDS_BIGENDIAN
.endif

LOCALSTATEDIR=	/var/run
SYSCONFDIR=	/etc

CFLAGS+=	-DNS_LOCALSTATEDIR='"${LOCALSTATEDIR}"'
CFLAGS+=	-DNS_SYSCONFDIR='"${SYSCONFDIR}"'
CFLAGS+=	-DNAMED_CONFFILE='"${SYSCONFDIR}/named.conf"'
CFLAGS+=	-DRNDC_CONFFILE='"${SYSCONFDIR}/rndc.conf"'
CFLAGS+=	-DRNDC_KEYFILE='"${SYSCONFDIR}/rndc.key"'

BIND_DPADD=	${LIBBIND9} ${LIBDNS} ${LIBISCCC} ${LIBISCCFG} \
		${LIBISC} ${LIBLWRES} ${LIBCRYPTO} ${LIBPTHREAD}
BIND_LDADD=	-lbind9 -ldns -lisccc -lisccfg -lisc -llwres \
		-lcrypto -lpthread
