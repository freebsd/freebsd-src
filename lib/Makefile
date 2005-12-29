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
# libnetgraph must be built before libbsnmp/modules/snmp_netgraph.
# libopie must be built before libpam.
# libradius must be built before libpam.
# librpcsvc must be built before libpam.
# libsbuf must be built before libcam.
# libtacplus must be built before libpam.
# libutil must be built before libpam.
# libypclnt must be built before libpam.
#
# Otherwise, the SUBDIR list should be in alphabetical order.

SUBDIR=	${_csu} libcom_err libcrypt libkvm msun libmd libncurses \
	libnetgraph libradius librpcsvc libsbuf libtacplus libutil \
	${_libypclnt} libalias libarchive ${_libatm} \
	libbegemot ${_libbluetooth} libbsnmp libbz2 libc ${_libc_r} \
	libcalendar libcam libcompat libdevinfo libdevstat ${_libdisk} \
	libedit libexpat libfetch libform libftpio libgeom ${_libgpib} \
	libgssapi ${_libio} libipsec \
	libipx libkiconv libmagic libmemstat libmenu ${_libmilter} ${_libmp} \
	${_libncp} ${_libngatm} libopie libpam libpanel libpcap \
	libpmc ${_libpthread} ${_libsdp} ${_libsm} ${_libsmb} ${_libsmdb} \
	${_libsmutil} libstand libtelnet ${_libthr} ${_libthread_db} libufs \
	libugidfw ${_libusbhid} ${_libvgl} libwrap liby libz ${_bind}

.if exists(${.CURDIR}/csu/${MACHINE_ARCH}-elf)
_csu=csu/${MACHINE_ARCH}-elf
.elif exists(${.CURDIR}/csu/${MACHINE_ARCH}/Makefile)
_csu=csu/${MACHINE_ARCH}
.else
_csu=csu
.endif

.if !defined(NO_ATM)
_libatm=	libatm
_libngatm=	libngatm
.endif

.if !defined(NO_BIND)
_bind=		bind
.endif

.if !defined(NO_BLUETOOTH)
_libbluetooth=	libbluetooth
_libsdp=	libsdp
.endif

.if ${MACHINE_ARCH} == "i386"
_libncp=	libncp
_libsmb=	libsmb
_libvgl=	libvgl
.endif

.if ${MACHINE_ARCH} != "arm" && ${MACHINE_ARCH} != "ia64" && \
    ${MACHINE_ARCH} != "powerpc" && !defined(NO_LIBC_R)
_libc_r=
.endif

.if ${MACHINE_ARCH} != "arm"
_libdisk=	libdisk
.endif

.if ${MACHINE_ARCH} == "alpha"
_libio=		libio
.endif

.if !defined(NO_SENDMAIL)
_libmilter=	libmilter
_libsm=		libsm
_libsmdb=	libsmdb
_libsmutil=	libsmutil
.endif

.if !defined(NO_CRYPT) && !defined(NO_OPENSSL)
_libmp=		libmp
.endif

.if ${MACHINE_ARCH} == "amd64"
_libncp=	libncp
_libsmb=	libsmb
.endif

.if ${MACHINE_ARCH} == "powerpc"
_libsmb=	libsmb
.endif

.if !defined(NO_LIBPTHREAD)
_libpthread=	libpthread
.endif

.if !defined(NO_LIBTHR)
_libthr= libthr
.endif

.if ${MACHINE_ARCH} != "arm" && ${MACHINE_ARCH} != "powerpc"
_libthread_db= libthread_db
.endif

.if !defined(NO_USB)
_libusbhid=	libusbhid
.endif

.if !defined(NO_NIS)
_libypclnt=	libypclnt
.endif

.if !defined(NO_GPIB)
_libgpib=	libgpib
.endif

.include <bsd.subdir.mk>
