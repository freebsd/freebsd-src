#	@(#)Makefile	8.1 (Berkeley) 6/4/93
# $FreeBSD$

# To satisfy shared library or ELF linkage when only the libraries being
# built are visible:
#
# csu must be built before all shared libaries for ELF.
# libcom_err must be built before libss.
# libcrypt must be built before libskey.
# libm must be built before libf2c, libg++ and libstdc++.
# libmd must be built before libatm, libopie, libradius, libskey,
# and libtacplus.
# libmytinfo must be built before libdialog and libncurses.
# libncurses must be built before libdialog.
# libradius must be built before libpam.
# libskey must be built before libpam.
# libtacplus must be built before libpam.
# libtermcap must be built before libcurses, libedit and libreadline.
#
# Otherwise, the SUBDIR list should be in alphabetical order.

SUBDIR=	${_csu} libcom_err ${_libcrypt} ${_libm} libmd libmytinfo \
	libncurses libradius libskey libtacplus libtermcap \
	${_compat} libalias libatm ${_libbind} libc ${_libc_r} libcalendar \
	libcam libcompat libcurses libdevstat libdisk libedit libf2c libfetch \
	libftpio libgnumalloc ${_libio} libipx libisc libkvm libnetgraph \
	libopie libpam libpcap libresolv librpcsvc libss libstand \
	${_libtelnet} libutil ${_libvgl} libwrap libxpg4 liby libz

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

# Build both libraries. They have different names, so no harm,
# and this avoids having stale libscrypt.*
.if exists(${.CURDIR}/../secure) && !defined(NOSECURE) && !defined(NOCRYPT)
_libcrypt=	../secure/lib/libcrypt libcrypt
.else
_libcrypt=	libcrypt
.endif

.if ${MACHINE_ARCH} == "i386"
_compat=	compat
_libvgl=	libvgl
.endif

.if defined(WANT_CSRG_LIBM)
_libm=		libm
.else
_libm=		msun
.endif

.if ${MACHINE_ARCH} == "alpha"
_libio=		libio
.endif

.if defined(RELEASEDIR) || !exists(${.CURDIR}/../kerberosIV) || \
    defined(NOCRYPT) || !defined(MAKE_KERBEROS4)
_libtelnet=	libtelnet
.endif

.include <bsd.subdir.mk>
