#	@(#)Makefile	8.1 (Berkeley) 6/4/93
#	$Id: Makefile,v 1.82 1998/10/13 16:48:43 bde Exp $

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
# libtermcap must be built before libcurses, libedit and libreadline.
#
# Otherwise, the SUBDIR list should be in alphabetical order.

SUBDIR=	${_csu} libcom_err ${_libcrypt} ${_libm} libmd libmytinfo \
	libncurses libtermcap \
	${_compat} libalias libatm libbind libc ${_libc_r} libcalendar libcam \
	libcompat libcurses libdevstat libdisk libedit libf2c libfetch \
	libftpio libgnumalloc libipx libkvm libopie \
	libpcap libradius libresolv librpcsvc libskey libss libstand \
	libtacplus ${_libtelnet} libutil ${_libvgl} libxpg4 liby libz

.if exists(${.CURDIR}/csu/${MACHINE_ARCH}-${OBJFORMAT})
_csu=csu/${MACHINE_ARCH}-${OBJFORMAT}
.elif exists(${.CURDIR}/csu/${MACHINE_ARCH})
_csu=csu/${MACHINE_ARCH}
.endif

.if !defined(NOLIBC_R)
_libc_r=	libc_r
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

.if defined(RELEASEDIR) || !exists(${.CURDIR}/../kerberosIV) || \
    defined(NOCRYPT) || !defined(MAKE_KERBEROS4)
_libtelnet=	libtelnet
.endif

.include <bsd.subdir.mk>
