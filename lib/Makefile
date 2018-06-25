#	@(#)Makefile	8.1 (Berkeley) 6/4/93
# $FreeBSD$

.include <src.opts.mk>

# The SUBDIR_BOOTSTRAP list is a small set of libraries which are used by many
# of the other libraries.  These are built first with a .WAIT between them
# and the main list to avoid needing a SUBDIR_DEPEND line on every library
# naming just these few items.

SUBDIR_BOOTSTRAP= \
	csu \
	.WAIT \
	libc \
	libc_nonshared \
	libcompiler_rt \
	${_libclang_rt} \
	${_libcplusplus} \
	${_libcxxrt} \
	libelf \
	msun

# The main list; please keep these sorted alphabetically.

SUBDIR=	${SUBDIR_BOOTSTRAP} \
	.WAIT \
	libalias \
	libarchive \
	${_libatm} \
	libauditd \
	libbegemot \
	${_libblacklist} \
	libblocksruntime \
	${_libbluetooth} \
	${_libbsnmp} \
	libbsdstat \
	libbsm \
	libbz2 \
	libcalendar \
	libcam \
	libcapsicum \
	${_libcasper} \
	${_libcom_err} \
	libcompat \
	libcrypt \
	libdevctl \
	libdevdctl \
	libdevinfo \
	libdevstat \
	${_libdl} \
	libdwarf \
	libedit \
	${_libefivar} \
	libelftc \
	libevent \
	libexecinfo \
	libexpat \
	libfetch \
	libfigpar \
	libgeom \
	${_libgpio} \
	${_libgssapi} \
	${_librpcsec_gss} \
	${_libiconv_modules} \
	libipsec \
	libjail \
	libkiconv \
	libkvm \
	${_libldns} \
	liblzma \
	${_libmagic} \
	libmemstat \
	libmd \
	${_libmilter} \
	${_libmp} \
	libmt \
	${_libnandfs} \
	lib80211 \
	libnetbsd \
	${_libnetgraph} \
	${_libngatm} \
	libnv \
	libopenbsd \
	libopie \
	libpam \
	${_libpcap} \
	${_libpe} \
	libpjdlog \
	${_libpmc} \
	${_libproc} \
	libprocstat \
	${_libradius} \
	librpcsvc \
	librt \
	${_librtld_db} \
	libsbuf \
	${_libsdp} \
	${_libsm} \
	libsmb \
	${_libsmdb} \
	${_libsmutil} \
	libsqlite3 \
	libstdbuf \
	libstdthreads \
	libsysdecode \
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
	libxo \
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
SUBDIR_DEPEND_libc++:= libcxxrt
SUBDIR_DEPEND_libc= libcompiler_rt
SUBDIR_DEPEND_libcam= libsbuf
SUBDIR_DEPEND_libcasper= libnv
SUBDIR_DEPEND_libdevstat= libkvm
SUBDIR_DEPEND_libdpv= libfigpar ncurses libutil
SUBDIR_DEPEND_libedit= ncurses
SUBDIR_DEPEND_libgeom= libexpat libsbuf
SUBDIR_DEPEND_librpcsec_gss= libgssapi
SUBDIR_DEPEND_libmagic= libz
SUBDIR_DEPEND_libmemstat= libkvm
SUBDIR_DEPEND_libopie= libmd
SUBDIR_DEPEND_libpam= libcrypt libopie ${_libradius} librpcsvc libtacplus libutil ${_libypclnt} ${_libcom_err} 
SUBDIR_DEPEND_libpjdlog= libutil
SUBDIR_DEPEND_libprocstat= libkvm libutil
SUBDIR_DEPEND_libradius= libmd
SUBDIR_DEPEND_libsmb= libkiconv
SUBDIR_DEPEND_libtacplus= libmd
SUBDIR_DEPEND_libulog= libmd
SUBDIR_DEPEND_libunbound= ${_libldns}
SUBDIR_DEPEND_liblzma= ${_libthr}

# NB: keep these sorted by MK_* knobs

.if ${MK_ATM} != "no"
_libngatm=	libngatm
.endif

.if ${MK_BLACKLIST} != "no"
_libblacklist=	libblacklist
.endif

.if ${MK_BLUETOOTH} != "no"
_libbluetooth=	libbluetooth
_libsdp=	libsdp
.endif

.if ${MK_BSNMP} != "no"
_libbsnmp=	libbsnmp
.endif

.if ${MK_CASPER} != "no"
_libcasper=	libcasper
.endif

.if ${MK_CLANG} != "no" && !defined(COMPAT_32BIT) && !defined(COMPAT_SOFTFP)
_clang=		clang
.endif

.if ${MK_CUSE} != "no"
_cuse=		libcuse
.endif

.if ${MK_TOOLCHAIN} != "no"
_libpe=		libpe
.endif

SUBDIR.${MK_DIALOG}+=	libdpv

.if ${MK_FILE} != "no"
_libmagic=	libmagic
.endif

.if ${MK_GPIO} != "no"
_libgpio=	libgpio
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

# The libraries under libclang_rt can only be built by clang, and only make
# sense to build when clang is enabled at all.  Furthermore, they can only be
# built for certain architectures.
.if ${MK_CLANG} != "no" && ${COMPILER_TYPE} == "clang" && \
    (${MACHINE_CPUARCH} == "aarch64" || ${MACHINE_CPUARCH} == "amd64" || \
    (${MACHINE_CPUARCH} == "arm" && ${MACHINE_ARCH} != "armeb") || \
    (${MACHINE_CPUARCH} == "i386"))
_libclang_rt=	libclang_rt
.endif

.if ${MK_LIBCPLUSPLUS} != "no"
_libcxxrt=	libcxxrt
_libcplusplus=	libc++
.endif

.if ${MK_EFI} != "no"
_libefivar=	libefivar
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
_libvgl=	libvgl
_libproc=	libproc
_librtld_db=	librtld_db
.endif

.if ${MACHINE_CPUARCH} == "amd64"
.if ${MK_BHYVE} != "no"
_libvmmapi=	libvmmapi
.endif
.endif

.if ${MACHINE_CPUARCH} == "mips"
_libproc=	libproc
_librtld_db=	librtld_db
.endif

.if ${MK_OFED} == "no"
#
# When OFED is enabled libpcap is built as part of
# OFED due to library dependencies
#
_libpcap=	libpcap
.endif

.if ${MACHINE_CPUARCH} == "powerpc"
_libproc=	libproc
_librtld_db=	librtld_db
.endif

.if defined(LINKER_FEATURES) && ${LINKER_FEATURES:Mfilter}
_libdl=		libdl
.endif

.if ${MACHINE_CPUARCH} == "aarch64" || ${MACHINE_CPUARCH} == "arm" || \
    ${MACHINE_CPUARCH} == "riscv"
_libproc=	libproc
_librtld_db=	librtld_db
.endif

.if ${MK_OPENSSL} != "no"
_libmp=		libmp
.endif

.if ${MK_PMC} != "no"
_libpmc=	libpmc
.endif

.if ${MK_RADIUS_SUPPORT} != "no"
_libradius=	libradius
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

.if ${MK_TESTS_SUPPORT} != "no"
_atf=		atf
.endif
.if ${MK_TESTS} != "no"
_tests=		tests
.endif

.if ${MK_UNBOUND} != "no"
_libunbound=	libunbound
.endif

.if ${MK_USB} != "no"
_libusbhid=	libusbhid
_libusb=	libusb
.endif

.if !make(install)
SUBDIR_PARALLEL=
.endif

.include <bsd.subdir.mk>
