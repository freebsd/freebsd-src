#	@(#)Makefile	8.1 (Berkeley) 6/4/93
# $FreeBSD$

.include <bsd.own.mk>

# To satisfy shared library or ELF linkage when only the libraries being
# built are visible:
#
# csu must be built before all shared libaries for ELF.
# libc must be built before all other shared libraries.
# libcom_err must be built before libkrb5 and libpam.
# libcrypt must be built before libkrb5 and libpam.
# libkvm must be built before libdevstat.
# msun must be built before libg++ and libstdc++.
# libmd must be built before libatm, libopie, libradius, and libtacplus.
# ncurses must be built before libdialog, libedit and libreadline.
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

SUBDIR=	${_csu} libc libbsm libcom_err libcrypt libelf libkvm msun libmd \
	ncurses libnetgraph libradius librpcsvc libsbuf libtacplus libutil \
	${_libypclnt} libalias libarchive ${_libatm} \
	libbegemot ${_libbluetooth} libbsnmp libbz2 \
	libcalendar libcam libcompat libdevinfo libdevstat libdisk \
	libedit libexpat libfetch libftpio libgeom ${_libgpib} \
	${_libgssapi} libipsec \
	${_libipx} libkiconv libmagic libmemstat ${_libmilter} ${_libmp} \
	${_libncp} ${_libngatm} libopie libpam libpcap \
	libpmc ${_libkse} librt ${_libsdp} ${_libsm} ${_libsmb} \
	${_libsmdb} \
	${_libsmutil} libstand libtelnet ${_libthr} libthread_db libufs \
	libugidfw ${_libusbhid} ${_libvgl} libwrap liby libz ${_bind}

.if exists(${.CURDIR}/csu/${MACHINE_ARCH}-elf)
_csu=csu/${MACHINE_ARCH}-elf
.elif exists(${.CURDIR}/csu/${MACHINE_ARCH}/Makefile)
_csu=csu/${MACHINE_ARCH}
.else
_csu=csu
.endif

.if ${MK_ATM} != "no"
# Disabled in 7.0 as netatm is not MPSAFE.
#_libatm=	libatm
_libngatm=	libngatm
.endif

.if ${MK_BIND} != "no"
_bind=		bind
.endif

.if ${MK_BLUETOOTH} != "no"
_libbluetooth=	libbluetooth
_libsdp=	libsdp
.endif

.if ${MK_KERBEROS} != "no"
_libgssapi=	libgssapi
.else
.if ${MK_GSSAPI} == "yes"
_libgssapi=	libgssapi
.endif
.endif

.if ${MK_IPX} != "no"
_libipx=	libipx
.endif

.if ${MACHINE_ARCH} == "i386"
.if ${MK_NCP} != "no"
_libncp=	libncp
.endif
_libsmb=	libsmb
_libvgl=	libvgl
.endif

.if ${MACHINE_ARCH} == "ia64"
_libsmb=	libsmb
.endif

.if ${MK_SENDMAIL} != "no"
_libmilter=	libmilter
_libsm=		libsm
_libsmdb=	libsmdb
_libsmutil=	libsmutil
.endif

.if ${MK_OPENSSL} != "no"
_libmp=		libmp
.endif

.if ${MACHINE_ARCH} == "amd64"
.if ${MK_NCP} != "no"
_libncp=	libncp
.endif
_libsmb=	libsmb
.endif

.if ${MACHINE_ARCH} == "powerpc"
_libsmb=	libsmb
.endif

.if ${MK_LIBKSE} != "no"
_libkse=	libkse
.endif

.if ${MK_LIBTHR} != "no"
_libthr=	libthr
.endif

.if ${MK_USB} != "no"
_libusbhid=	libusbhid
.endif

.if ${MK_NIS} != "no"
_libypclnt=	libypclnt
.endif

.if ${MK_GPIB} != "no"
_libgpib=	libgpib
.endif

.include <bsd.subdir.mk>
