#	@(#)Makefile	8.1 (Berkeley) 6/4/93
# $FreeBSD$

# To satisfy shared library or ELF linkage when only the libraries being
# built are visible:
#
# csu must be built before all shared libaries for ELF.
# libcom_err must be built before libkrb5 and libpam.
# libcrypt must be built before libkrb5 and libpam.
# libkvm must be built before libdevstat.
# msun must be built before libg++ and libstdc++.
# libmd must be built before libatm, libopie, libradius, and libtacplus.
# libncurses must be built before libdialog, libedit and libreadline.
# libopie must be built before libpam.
# libradius must be built before libpam.
# librpcsvc must be built before libpam.
# libsbuf must be built before libcam.
# libtacplus must be built before libpam.
# libutil must be built before libpam.
# libypclnt must be built before libpam.
#
# Otherwise, the SUBDIR list should be in alphabetical order.

SUBDIR=	${_csu} libcom_err libcrypt libkvm msun libmd \
	libncurses libradius librpcsvc libsbuf libtacplus libutil libypclnt \
	${_compat} libalias ${_libatm} ${_libbind} \
	${_libbluetooth} libbz2 libc ${_libc_r} \
	libcalendar libcam libcompat libdevinfo libdevstat ${_libdisk} \
	libedit libexpat libfetch libform libftpio libgeom ${_libio} libipsec \
	libipx libisc libkiconv libmenu ${_libmilter} ${_libmp} ${_libncp} \
	libnetgraph libopie libpam libpanel libpcap ${_libpthread} \
	${_libsdp} ${_libsm} ${_libsmb} ${_libsmdb} ${_libsmutil} \
	libstand libtelnet ${_libthr} libufs libugidfw ${_libusbhid} \
	${_libvgl} libwrap libxpg4 liby libz

.if exists(${.CURDIR}/csu/${MACHINE_ARCH}-elf)
_csu=csu/${MACHINE_ARCH}-elf
.elif exists(${.CURDIR}/csu/${MACHINE_ARCH}/Makefile)
_csu=csu/${MACHINE_ARCH}
.else
_csu=csu
.endif

# libc_r is obsolete on ia64.
.if ${MACHINE_ARCH} != "ia64" && ${MACHINE_ARCH} != "powerpc"
.if !defined(NOLIBC_R)
_libc_r=	libc_r
.endif
.endif

.if !defined(NOATM)
_libatm=	libatm
.endif

.if !defined(NO_BIND)
_libbind=	libbind
.endif

.if !defined(NO_SENDMAIL)
_libmilter=	libmilter
_libsm=		libsm
_libsmdb=	libsmdb
_libsmutil=	libsmutil
.endif

.if !defined(NO_USB)
_libusbhid=	libusbhid
.endif

.if ${MACHINE_ARCH} == "i386"
_compat=	compat
_libbluetooth=	libbluetooth
_libncp=	libncp
_libsdp=	libsdp
_libsmb=	libsmb
_libvgl=	libvgl
.endif

.if ${MACHINE_ARCH} == "alpha"
_libio=		libio
_compat=	compat
.endif

.if ${MACHINE_ARCH} == "amd64"
_libncp=	libncp
_libsmb=	libsmb
.endif

.if ${MACHINE_ARCH} != "powerpc" && ${MACHINE_ARCH} != "sparc64"
.if !defined(NOLIBPTHREAD)
_libpthread=	libpthread
.endif
.endif

.if ${MACHINE_ARCH} != "amd64" && ${MACHINE_ARCH} != "powerpc"
.if !defined(NOLIBTHR)
_libthr=	libthr
.endif
.endif

.if ${MACHINE_ARCH} != "powerpc"
_libdisk=	libdisk
.endif

.if !defined(NOCRYPT) && !defined(NO_OPENSSL)
_libmp=		libmp
.endif

.include <bsd.subdir.mk>
