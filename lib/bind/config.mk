# $FreeBSD$

.include <bsd.own.mk>

# BIND version number
.if defined(BIND_DIR) && exists(${BIND_DIR}/version)
.include	"${BIND_DIR}/version"
BIND_VERSION=	${MAJORVER}.${MINORVER}.${PATCHVER}${RELEASETYPE}${RELEASEVER}
CFLAGS+=	-DVERSION='"${BIND_VERSION}"'
.endif

CFLAGS+=	-DHAVE_CONFIG_H
CFLAGS+=	-D_REENTRANT -D_THREAD_SAFE

# Get version numbers (for libraries)
.if defined(SRCDIR) && exists(${SRCDIR}/api)
.include	"${SRCDIR}/api"
CFLAGS+=	-DLIBINTERFACE=${LIBINTERFACE}
CFLAGS+=	-DLIBREVISION=${LIBREVISION}
CFLAGS+=	-DLIBAGE=${LIBAGE}
.if ${MK_BIND_LIBS} != "no"
SHLIB_MAJOR=	${LIBINTERFACE}
SHLIB_MINOR=	${LIBINTERFACE}
.else
INTERNALLIB=
.endif
.endif

# GSSAPI support is incomplete in 9.3.0
#.if ${MK_KERBEROS} != "no"
#CFLAGS+=	-DGSSAPI
#.endif

# Enable IPv6 support if available
.if ${MK_INET6_SUPPORT} != "no"
CFLAGS+=	-DWANT_IPV6
.endif

# Enable crypto if available
.if ${MK_OPENSSL} != "no"
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

# Use the right version of the atomic.h file from lib/isc
.if ${MACHINE_ARCH} == "amd64" || ${MACHINE_ARCH} == "i386"
ISC_ATOMIC_ARCH=	x86_32
.else
ISC_ATOMIC_ARCH=	${MACHINE_ARCH}
.endif

# Optional features
.if ${MK_BIND_LARGE_FILE} == "yes"
CFLAGS+=	-D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64
.endif
.if ${MK_BIND_SIGCHASE} == "yes"
CFLAGS+=	-DDIG_SIGCHASE
.endif

# Link against BIND libraries
.if ${MK_BIND_LIBS} == "no"
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
.if ${MK_BIND_LIBS} != "no"
BIND_LDADD=	-lbind9 -ldns -lisccc -lisccfg -lisc -llwres
CFLAGS+=	-I${BIND_DIR}/lib/isc/include
CFLAGS+=	-I${BIND_DIR}/lib/isc/unix/include
CFLAGS+=	-I${BIND_DIR}/lib/isc/pthreads/include
CFLAGS+=	-I${.CURDIR}/../dns
CFLAGS+=	-I${BIND_DIR}/lib/dns/include
CFLAGS+=	-I${BIND_DIR}/lib/isccfg/include
CFLAGS+=	-I${.CURDIR}/../isc
.else
BIND_LDADD=	${BIND_DPADD}
.endif

# Link against crypto library
.if ${MK_OPENSSL} != "no"
CRYPTO_DPADD=	${LIBCRYPTO}
CRYPTO_LDADD=	-lcrypto
.endif

.if ${MK_BIND_XML} == "yes"
CFLAGS+=	-DHAVE_LIBXML2
CFLAGS+=	-I/usr/local/include -I/usr/local/include/libxml2
.if ${MK_BIND_LIBS} != "no"
BIND_LDADD+=	-L/usr/local/lib -lxml2 -lz -liconv -lm
.else
BIND_DPADD+=	/usr/local/lib/libxml2.a ${LIBZ} 
BIND_DPADD+=	/usr/local/lib/libiconv.a ${LIBM}
.endif
.endif

PTHREAD_DPADD=	${LIBPTHREAD}
PTHREAD_LDADD=	-lpthread
