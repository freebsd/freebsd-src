#	@(#)Makefile	8.1 (Berkeley) 6/4/93
#	$Id$

# To satisfy shared library or ELF linkage when only the libraries being
# built are visible:
#
# libcom_err must be built before libss.
# libcrypt and libmd must be built before libskey.
# libm must be built before libtcl.
# libmytinfo must be built before libncurses.
# libtermcap must be built before libcurses and libedit.
#
# Otherwise, the SUBDIR list should be in alphabetical order.

SUBDIR=	libcom_err ${_libcrypt} ${_libm} libmytinfo libtermcap \
	compat ${_csu} libalias libc ${_libc_r} libcalendar \
	libcompat libcurses ${_libdisk} libedit libf2c libftpio \
	libgnumalloc libipx ${_libkvm} libmd libncurses libopie \
	libpcap libresolv librpcsvc ${_libscsi} libskey libss \
	${_libtcl} ${_libtelnet} libutil ${_libvgl} libxpg4 liby libz

.if exists(${.CURDIR}/csu/${MACHINE_ARCH}-${BINFORMAT})
_csu=csu/${MACHINE_ARCH}-${BINFORMAT}
.elif exists(${.CURDIR}/csu/${MACHINE_ARCH})
_csu=csu/${MACHINE_ARCH}
.endif

.if !defined(NOLIBC_R)
_libc_r=	libc_r
.endif

# Build both libraries. They have different names, so no harm,
# and this avoids having stale libscrypt.*
_libcrypt=	libcrypt
.if exists(${.CURDIR}/../secure) && !defined(NOSECURE) && !defined(NOCRYPT)
_libcrypt+=	../secure/lib/libcrypt
.endif

.if ${MACHINE_ARCH} == "i386"
_libdisk=	libdisk
_libkvm=	libkvm
_libscsi=	libscsi
_libvgl=	libvgl

.if !defined(NOTCL) && exists (${.CURDIR}/../contrib/tcl) && \
    exists(${.CURDIR}/../usr.bin/tclsh) && exists (${.CURDIR}/libtcl)
_libtcl=	libtcl
.endif
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
