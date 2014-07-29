#	@(#)Makefile	8.1 (Berkeley) 6/4/93
# $FreeBSD$

.include <src.opts.mk>

# The SUBDIR_ORDERED list is a small set of libraries which are used by many
# of the other libraries.  These are built first with a .WAIT between them
# and the main list to avoid needing a SUBDIR_DEPEND line on every library
# naming just these few items.

SUBDIR_ORDERED=	${_csu} \
	.WAIT \
	libc \
	libc_nonshared \
	libcompiler_rt \
	${_libcplusplus} \
	${_libcxxrt} \
	libelf \
	msun

# The main list; please keep these sorted alphabetically.

SUBDIR=	${SUBDIR_ORDERED} \
	.WAIT \
	libalias \
	libarchive \
	${_libatm} \
	libauditd \
	libbegemot \
	libblocksruntime \
	${_libbluetooth} \
	${_libbsnmp} \
	libbsdstat \
	libbsm \
	libbz2 \
	libcalendar \
	libcam \
	${_libcapsicum} \
	${_libcasper} \
	${_libcom_err} \
	libcompat \
	libcrypt \
	libdevinfo \
	libdevstat \
	libdwarf \
	libedit \
	libexecinfo \
	libexpat \
	libfetch \
	libgeom \
	${_libgpib} \
	${_libgssapi} \
	${_librpcsec_gss} \
	${_libiconv_modules} \
	libipsec \
	libjail \
	libkiconv \
	libkvm \
	${_libldns} \
	liblzma \
	libmagic \
	libmandoc \
	libmemstat \
	libmd \
	${_libmilter} \
	${_libmp} \
	${_libnandfs} \
	libnetbsd \
	${_libnetgraph} \
	${_libngatm} \
	libnv \
	libopie \
	libpam \
	libpcap \
	libpjdlog \
	${_libpmc} \
	${_libproc} \
	libprocstat \
	libradius \
	librpcsvc \
	librt \
	${_librtld_db} \
	libsbuf \
	${_libsdp} \
	${_libsm} \
	${_libsmb} \
	${_libsmdb} \
	${_libsmutil} \
	libstand \
	libstdbuf \
	libstdthreads \
	libtacplus \
	${_libtelnet} \
	${_libthr} \
	libthread_db \
	libucl \
	libufs \
	libugidfw \
	libulog \
	${_libunbound} \
	${_libusbhid} \
	${_libusb} \
	libutil \
	${_libvgl} \
	${_libvmmapi} \
	libwrap \
	liby \
	${_libypclnt} \
	libz \
	ncurses \
	${_atf} \
	${_clang} \
	${_cuse} \
	${_tests}

# Inter-library dependencies.  When the makefile for a library contains LDADD
# libraries, those libraries should be listed as build order dependencies here.

SUBDIR_DEPEND_libarchive= libz libbz2 libexpat liblzma libmd
SUBDIR_DEPEND_libatm= libmd
SUBDIR_DEPEND_libauditdm= libbsm
SUBDIR_DEPEND_libbsnmp= ${_libnetgraph}
SUBDIR_DEPEND_libc++= libcxxrt
SUBDIR_DEPEND_libc= libcompiler_rt
SUBDIR_DEPEND_libcam= libsbuf
SUBDIR_DEPEND_libcapsicum= libnv
SUBDIR_DEPEND_libcasper= libcapsicum libnv libpjdlog
SUBDIR_DEPEND_libdevstat= libkvm
SUBDIR_DEPEND_libdiaglog= ncurses
SUBDIR_DEPEND_libedit= ncurses
SUBDIR_DEPEND_libg++= msun
SUBDIR_DEPEND_libgeom= libexpat libsbuf
SUBDIR_DEPEND_liblibrpcsec_gss= libgssapi
SUBDIR_DEPEND_libmagic= libz
SUBDIR_DEPEND_libmemstat= libkvm
SUBDIR_DEPEND_libopie= libmd
SUBDIR_DEPEND_libpam= libcrypt libopie libradius librpcsvc libtacplus libutil ${_libypclnt} ${_libcom_err} 
SUBDIR_DEPEND_libpjdlog= libutil
SUBDIR_DEPEND_libprocstat= libkvm libutil
SUBDIR_DEPEND_libradius= libmd
SUBDIR_DEPEND_libreadline= ncurses
SUBDIR_DEPEND_libsmb= libkiconv
SUBDIR_DEPEND_libstdc++= msun
SUBDIR_DEPEND_libtacplus= libmd
SUBDIR_DEPEND_libulog= libmd
SUBDIR_DEPEND_libunbound= ${_libldns}

.if exists(${.CURDIR}/csu/${MACHINE_ARCH}-elf)
_csu=csu/${MACHINE_ARCH}-elf
.elif exists(${.CURDIR}/csu/${MACHINE_ARCH})
_csu=csu/${MACHINE_ARCH}
.elif exists(${.CURDIR}/csu/${MACHINE_CPUARCH}/Makefile)
_csu=csu/${MACHINE_CPUARCH}
.else
_csu=csu
.endif

# NB: keep these sorted by MK_* knobs

.if ${MK_ATM} != "no"
_libngatm=	libngatm
.endif

.if ${MK_BLUETOOTH} != "no"
_libbluetooth=	libbluetooth
_libsdp=	libsdp
.endif

.if ${MK_BSNMP} != "no"
_libbsnmp=	libbsnmp
.endif

.if ${MK_CASPER} != "no"
_libcapsicum=	libcapsicum
_libcasper=	libcasper
.endif

.if ${MK_CLANG} != "no" && !defined(COMPAT_32BIT)
_clang=		clang
.endif

.if ${MK_CUSE} != "no"
_cuse=		libcuse
.endif

.if ${MK_GPIB} != "no"
_libgpib=	libgpib
.endif

.if ${MK_GSSAPI} != "no"
_libgssapi=	libgssapi
_librpcsec_gss=	librpcsec_gss
.endif

.if ${MK_ICONV} != "no"
_libiconv_modules=	libiconv_modules
.endif

.if ${MK_KERBEROS_SUPPORT} != "no"
_libcom_err= libcom_err
.endif

.if ${MK_LDNS} != "no"
_libldns=	libldns
.endif

.if ${MK_LIBCPLUSPLUS} != "no"
_libcxxrt=	libcxxrt
_libcplusplus=	libc++
.endif

.if ${MK_LIBTHR} != "no"
_libthr=	libthr
.endif

.if ${MK_NAND} != "no"
_libnandfs=	libnandfs
.endif

.if ${MK_NETGRAPH} != "no"
_libnetgraph=	libnetgraph
.endif

.if ${MK_NIS} != "no"
_libypclnt=	libypclnt
.endif

.if ${MACHINE_CPUARCH} == "i386" || ${MACHINE_CPUARCH} == "amd64"
_libsmb=	libsmb
_libvgl=	libvgl
_libproc=	libproc
_librtld_db=	librtld_db
.endif

.if ${MACHINE_CPUARCH} == "amd64"
_libvmmapi=	libvmmapi
.endif

.if ${MACHINE_CPUARCH} == "mips"
_libproc=	libproc
_librtld_db=	librtld_db
.endif

.if ${MACHINE_CPUARCH} == "powerpc"
_libproc=	libproc
_librtld_db=	librtld_db
_libsmb=	libsmb
.endif

.if ${MACHINE_CPUARCH} == "sparc64"
_libsmb=	libsmb
.endif

.if ${MK_OPENSSL} != "no"
_libmp=		libmp
.endif

.if ${MK_PMC} != "no"
_libpmc=	libpmc
.endif

.if ${MK_SENDMAIL} != "no"
_libmilter=	libmilter
_libsm=		libsm
_libsmdb=	libsmdb
_libsmutil=	libsmutil
.endif

.if ${MK_TELNET} != "no"
_libtelnet=	libtelnet
.endif

.if ${MK_TESTS} != "no"
_atf=		atf
_tests=		tests
.endif

.if ${MK_UNBOUND} != "no"
_libunbound=	libunbound
.endif

.if ${MK_USB} != "no"
_libusbhid=	libusbhid
_libusb=	libusb
.endif

.if !defined(LIBRARIES_ONLY)
afterinstall:
	${INSTALL_SYMLINK} ../include ${DESTDIR}/usr/lib/include
.endif

.if !make(install)
SUBDIR_PARALLEL=
.endif

.include <bsd.subdir.mk>
