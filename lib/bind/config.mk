# $FreeBSD$

# BIND version number
.if defined(BIND_DIR) && exists(${BIND_DIR}/version)
.include	"${BIND_DIR}/version"
BIND_VERSION=	${MAJORVER}.${MINORVER}.${PATCHVER}${RELEASETYPE}${RELEASEVER}
CFLAGS+=	-DVERSION='"${BIND_VERSION}"'
.endif

CFLAGS+=	-DHAVE_CONFIG_H

# Get version numbers (for libraries)
.if defined(SRCDIR) && exists(${SRCDIR}/api)
.include	"${SRCDIR}/api"
CFLAGS+=	-DLIBINTERFACE=${LIBINTERFACE}
CFLAGS+=	-DLIBREVISION=${LIBREVISION}
CFLAGS+=	-DLIBAGE=${LIBAGE}
.if defined(WITH_BIND_LIBS)
SHLIB_MAJOR=	${LIBINTERFACE}
SHLIB_MINOR=	${LIBINTERFACE}
.else
INTERNALLIB=	YES
.endif
.endif

# GSSAPI support is incomplete in 9.3.0
#.if !defined(NO_KERBEROS)
#CFLAGS+=	-DGSSAPI
#.endif

# Enable IPv6 support if available
.if !defined(NOINET6)
CFLAGS+=	-DWANT_IPV6
.endif

# Enable crypto if available
.if !defined(NOCRYPT)
CFLAGS+=	-DOPENSSL
.endif

# Enable MD5 - BIND has its own implementation
CFLAGS+=	-DUSE_MD5

# Endianness
.if ${MACHINE_ARCH} == "powerpc" || ${MACHINE_ARCH} == "sparc64"
CFLAGS+=	-DWORDS_BIGENDIAN
.endif

# Default file locations
LOCALSTATEDIR=	/var
SYSCONFDIR=	/etc/namedb
CFLAGS+=	-DNS_LOCALSTATEDIR='"${LOCALSTATEDIR}"'
CFLAGS+=	-DNS_SYSCONFDIR='"${SYSCONFDIR}"'
CFLAGS+=	-DNAMED_CONFFILE='"${SYSCONFDIR}/named.conf"'
CFLAGS+=	-DRNDC_CONFFILE='"${SYSCONFDIR}/rndc.conf"'
CFLAGS+=	-DRNDC_KEYFILE='"${SYSCONFDIR}/rndc.key"'

# Add correct include path for config.h
.if defined(LIB_BIND_DIR) && exists(${LIB_BIND_DIR}/config.h)
CFLAGS+=	-I${LIB_BIND_DIR}
.endif

# Link against BIND libraries
.if !defined(WITH_BIND_LIBS)
LIBBIND9=	${LIB_BIND_REL}/bind9/libbind9.a
CFLAGS+=	-I${BIND_DIR}/lib/bind9/include
LIBDNS=		${LIB_BIND_REL}/dns/libdns.a
CFLAGS+=	-I${BIND_DIR}/lib/dns/include/dst \
		-I${BIND_DIR}/lib/dns/include \
		-I${LIB_BIND_DIR}/dns
LIBISCCC=	${LIB_BIND_REL}/isccc/libisccc.a
CFLAGS+=	-I${BIND_DIR}/lib/isccc/include
LIBISCCFG=	${LIB_BIND_REL}/isccfg/libisccfg.a
CFLAGS+=	-I${BIND_DIR}/lib/isccfg/include
LIBISC=		${LIB_BIND_REL}/isc/libisc.a
CFLAGS+=	-I${BIND_DIR}/lib/isc/unix/include \
		-I${BIND_DIR}/lib/isc/pthreads/include \
		-I${BIND_DIR}/lib/isc/include \
		-I${LIB_BIND_DIR}/isc
LIBLWRES=	${LIB_BIND_REL}/lwres/liblwres.a
CFLAGS+=	-I${BIND_DIR}/lib/lwres/unix/include \
		-I${BIND_DIR}/lib/lwres/include \
		-I${LIB_BIND_DIR}/lwres
.endif
BIND_DPADD=	${LIBBIND9} ${LIBDNS} ${LIBISCCC} ${LIBISCCFG} \
		${LIBISC} ${LIBLWRES}
.if defined(WITH_BIND_LIBS)
BIND_LDADD=	-lbind9 -ldns -lisccc -lisccfg -lisc -llwres
.else
BIND_LDADD=	${BIND_DPADD}
.endif

# Link against crypto library
.if !defined(NOCRYPT)
CRYPTO_DPADD=	${LIBCRYPTO}
CRYPTO_LDADD=	-lcrypto
.endif

# Link against POSIX threads library
.if ${MACHINE_ARCH} == "alpha" || ${MACHINE_ARCH} == "sparc64"
.if defined(NOLIBC_R)
.error "BIND requires libpthread - define NO_BIND, or undefine NOLIBC_R"
.endif
.else
.if defined(NOLIBPTHREAD)
.error "BIND requires libpthread - define NO_BIND, or undefine NOLIBPTHREAD"
.endif
.endif

PTHREAD_DPADD=	${LIBPTHREAD}
PTHREAD_LDADD=	-lpthread
