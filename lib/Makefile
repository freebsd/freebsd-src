#	@(#)Makefile	8.1 (Berkeley) 6/4/93
# $FreeBSD$

# To satisfy shared library or ELF linkage when only the libraries being
# built are visible:
#
# csu must be built before all shared libaries for ELF.
# libcom_err must be built before libkrb, libpam and libss.
# libcrypt must be built before libkrb, libpam and libskey.
# msun must be built before libg++ and libstdc++.
# libmd must be built before libatm, libopie, libradius, libskey, and
# libtacplus.
# libncurses must be built before libdialog, libedit and libreadline.
# libopie must be built before libpam.
# libradius must be built before libpam.
# libskey must be built before libpam.
# libtacplus must be built before libpam.
# libutil must be built before libpam.
#
# Otherwise, the SUBDIR list should be in alphabetical order.

SUBDIR=	${_csu} libcom_err ${_libcrypt} msun libmd \
	libncurses libradius libskey libtacplus libutil \
	${_compat} libalias libatm ${_libbind} libc ${_libc_r} libcalendar \
	libcam libcompat libdevstat libdisk libedit libfetch libform \
	libftpio libgnumalloc ${_libio} libipsec libipx libkvm libmenu \
	${_libncp} \
	libnetgraph libopie libpam libpanel libpcap \
	libposix1e libresolv librpcsvc libsmdb libsmutil libss \
	libstand ${_libtelnet} libusb ${_libvgl} libwrap libxpg4 liby libz

.if exists(${.CURDIR}/csu/${MACHINE_ARCH}-${OBJFORMAT})
_csu=csu/${MACHINE_ARCH}-${OBJFORMAT}
.elif exists(${.CURDIR}/csu/${MACHINE_ARCH})
_csu=csu/${MACHINE_ARCH}
.endif

.if !defined(NOLIBC_R)
_libc_r=	libc_r
.endif

.if !defined(NO_BIND)
_libbind=	libbind
.endif

_libcrypt=	libcrypt
.if exists(${.CURDIR}/../secure) && !defined(NOSECURE) && !defined(NOCRYPT) && !defined(NO_DESCRYPT)
# Build both libraries. They have different names, so no harm,
# and this avoids having stale libscrypt.*
_libcrypt+=	../secure/lib/libcrypt
.endif

.if ${MACHINE_ARCH} == "i386"
_compat=	compat
_libncp=	libncp
_libvgl=	libvgl
.endif

.if ${MACHINE_ARCH} == "alpha"
_libio=		libio
.endif

.if defined(RELEASEDIR) || \
    (!exists(${.CURDIR}/../secure) && !exists(${.CURDIR}/../kerberosIV)) || \
    defined(NOCRYPT) || !defined(MAKE_KERBEROS4)
_libtelnet=	libtelnet
.endif

.include <bsd.subdir.mk>
