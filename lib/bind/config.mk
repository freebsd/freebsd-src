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
SHLIB_MAJOR=	${LIBINTERFACE}
CFLAGS+=	-DLIBREVISION=${LIBREVISION}
SHLIB_MINOR=	${LIBINTERFACE}
CFLAGS+=	-DLIBAGE=${LIBAGE}
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
BIND_DPADD=	${LIBBIND9} ${LIBDNS} ${LIBISCCC} ${LIBISCCFG} \
		${LIBISC} ${LIBLWRES}
BIND_LDADD=	-lbind9 -ldns -lisccc -lisccfg -lisc -llwres

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
