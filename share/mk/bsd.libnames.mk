# $FreeBSD$

# The include file <bsd.libnames.mk> define library names.
# Other include files (e.g. bsd.prog.mk, bsd.lib.mk) include this
# file where necessary.

.if !target(__<bsd.init.mk>__)
.error bsd.libnames.mk cannot be included directly.
.endif

.sinclude <src.libnames.mk>

# Src directory locations are also defined in src.libnames.mk.

LIBCRT0?=	${DESTDIR}${LIBDIR_BASE}/crt0.o

LIB80211?=	${DESTDIR}${LIBDIR_BASE}/lib80211.a
LIBALIAS?=	${DESTDIR}${LIBDIR_BASE}/libalias.a
LIBARCHIVE?=	${DESTDIR}${LIBDIR_BASE}/libarchive.a
LIBASN1?=	${DESTDIR}${LIBDIR_BASE}/libasn1.a
LIBATM?=	${DESTDIR}${LIBDIR_BASE}/libatm.a
LIBAUDITD?=	${DESTDIR}${LIBDIR_BASE}/libauditd.a
LIBAVL?=	${DESTDIR}${LIBDIR_BASE}/libavl.a
LIBBEGEMOT?=	${DESTDIR}${LIBDIR_BASE}/libbegemot.a
LIBBLACKLIST?=	${DESTDIR}${LIBDIR_BASE}/libblacklist.a
LIBBLUETOOTH?=	${DESTDIR}${LIBDIR_BASE}/libbluetooth.a
LIBBSDXML?=	${DESTDIR}${LIBDIR_BASE}/libbsdxml.a
LIBBSM?=	${DESTDIR}${LIBDIR_BASE}/libbsm.a
LIBBSNMP?=	${DESTDIR}${LIBDIR_BASE}/libbsnmp.a
LIBBZ2?=	${DESTDIR}${LIBDIR_BASE}/libbz2.a
LIBC?=		${DESTDIR}${LIBDIR_BASE}/libc.a
LIBCALENDAR?=	${DESTDIR}${LIBDIR_BASE}/libcalendar.a
LIBCAM?=	${DESTDIR}${LIBDIR_BASE}/libcam.a
LIBCAP_DNS?=	${DESTDIR}${LIBDIR_BASE}/libcap_dns.a
LIBCAP_GRP?=	${DESTDIR}${LIBDIR_BASE}/libcap_grp.a
LIBCAP_PWD?=	${DESTDIR}${LIBDIR_BASE}/libcap_pwd.a
LIBCAP_RANDOM?=	${DESTDIR}${LIBDIR_BASE}/libcap_random.a
LIBCAP_SYSCTL?=	${DESTDIR}${LIBDIR_BASE}/libcap_sysctl.a
LIBCASPER?=	${DESTDIR}${LIBDIR_BASE}/libcasper.a
LIBCOMPAT?=	${DESTDIR}${LIBDIR_BASE}/libcompat.a
LIBCOMPILER_RT?=${DESTDIR}${LIBDIR_BASE}/libcompiler_rt.a
LIBCOM_ERR?=	${DESTDIR}${LIBDIR_BASE}/libcom_err.a
LIBCPLUSPLUS?=	${DESTDIR}${LIBDIR_BASE}/libc++.a
LIBCRYPT?=	${DESTDIR}${LIBDIR_BASE}/libcrypt.a
LIBCRYPTO?=	${DESTDIR}${LIBDIR_BASE}/libcrypto.a
LIBCTF?=	${DESTDIR}${LIBDIR_BASE}/libctf.a
LIBCURSES?=	${DESTDIR}${LIBDIR_BASE}/libcurses.a
LIBCUSE?=	${DESTDIR}${LIBDIR_BASE}/libcuse.a
LIBCXGB4?=	${DESTDIR}${LIBDIR_BASE}/libcxgb4.a
LIBCXXRT?=	${DESTDIR}${LIBDIR_BASE}/libcxxrt.a
LIBC_PIC?=	${DESTDIR}${LIBDIR_BASE}/libc_pic.a
LIBDEVCTL?=	${DESTDIR}${LIBDIR_BASE}/libdevctl.a
LIBDEVDCTL?=	${DESTDIR}${LIBDIR_BASE}/libdevdctl.a
LIBDEVINFO?=	${DESTDIR}${LIBDIR_BASE}/libdevinfo.a
LIBDEVSTAT?=	${DESTDIR}${LIBDIR_BASE}/libdevstat.a
LIBDIALOG?=	${DESTDIR}${LIBDIR_BASE}/libdialog.a
LIBDL?=		${DESTDIR}${LIBDIR_BASE}/libdl.a
LIBDNS?=	${DESTDIR}${LIBDIR_BASE}/libdns.a
LIBDPV?=	${DESTDIR}${LIBDIR_BASE}/libdpv.a
LIBDTRACE?=	${DESTDIR}${LIBDIR_BASE}/libdtrace.a
LIBDWARF?=	${DESTDIR}${LIBDIR_BASE}/libdwarf.a
LIBEDIT?=	${DESTDIR}${LIBDIR_BASE}/libedit.a
LIBEFIVAR?=	${DESTDIR}${LIBDIR_BASE}/libefivar.a
LIBELF?=	${DESTDIR}${LIBDIR_BASE}/libelf.a
LIBEXECINFO?=	${DESTDIR}${LIBDIR_BASE}/libexecinfo.a
LIBFETCH?=	${DESTDIR}${LIBDIR_BASE}/libfetch.a
LIBFIGPAR?=	${DESTDIR}${LIBDIR_BASE}/libfigpar.a
LIBFL?=		"don't use LIBFL, use LIBL"
LIBFORM?=	${DESTDIR}${LIBDIR_BASE}/libform.a
LIBG2C?=	${DESTDIR}${LIBDIR_BASE}/libg2c.a
LIBGEOM?=	${DESTDIR}${LIBDIR_BASE}/libgeom.a
LIBGNUREGEX?=	${DESTDIR}${LIBDIR_BASE}/libgnuregex.a
LIBGPIO?=	${DESTDIR}${LIBDIR_BASE}/libgpio.a
LIBGSSAPI?=	${DESTDIR}${LIBDIR_BASE}/libgssapi.a
LIBGSSAPI_KRB5?= ${DESTDIR}${LIBDIR_BASE}/libgssapi_krb5.a
LIBHDB?=	${DESTDIR}${LIBDIR_BASE}/libhdb.a
LIBHEIMBASE?=	${DESTDIR}${LIBDIR_BASE}/libheimbase.a
LIBHEIMNTLM?=	${DESTDIR}${LIBDIR_BASE}/libheimntlm.a
LIBHEIMSQLITE?=	${DESTDIR}${LIBDIR_BASE}/libheimsqlite.a
LIBHX509?=	${DESTDIR}${LIBDIR_BASE}/libhx509.a
LIBIBCM?=	${DESTDIR}${LIBDIR_BASE}/libibcm.a
LIBIBCOMMON?=	${DESTDIR}${LIBDIR_BASE}/libibcommon.a
LIBIBMAD?=	${DESTDIR}${LIBDIR_BASE}/libibmad.a
LIBIBSDP?=	${DESTDIR}${LIBDIR_BASE}/libibsdp.a
LIBIBUMAD?=	${DESTDIR}${LIBDIR_BASE}/libibumad.a
LIBIBVERBS?=	${DESTDIR}${LIBDIR_BASE}/libibverbs.a
LIBIPSEC?=	${DESTDIR}${LIBDIR_BASE}/libipsec.a
LIBJAIL?=	${DESTDIR}${LIBDIR_BASE}/libjail.a
LIBKADM5CLNT?=	${DESTDIR}${LIBDIR_BASE}/libkadm5clnt.a
LIBKADM5SRV?=	${DESTDIR}${LIBDIR_BASE}/libkadm5srv.a
LIBKAFS5?=	${DESTDIR}${LIBDIR_BASE}/libkafs5.a
LIBKDC?=	${DESTDIR}${LIBDIR_BASE}/libkdc.a
LIBKEYCAP?=	${DESTDIR}${LIBDIR_BASE}/libkeycap.a
LIBKICONV?=	${DESTDIR}${LIBDIR_BASE}/libkiconv.a
LIBKRB5?=	${DESTDIR}${LIBDIR_BASE}/libkrb5.a
LIBKVM?=	${DESTDIR}${LIBDIR_BASE}/libkvm.a
LIBL?=		${DESTDIR}${LIBDIR_BASE}/libl.a
LIBIBCM?=	${DESTDIR}${LIBDIR_BASE}/libibcm.a
LIBIBMAD?=	${DESTDIR}${LIBDIR_BASE}/libibmad.a
LIBIBNETDISC?= ${LIBDESTDIR}${LIBDIR_BASE}/libibnetdisc.
LIBIBUMAD?=	${DESTDIR}${LIBDIR_BASE}/libibumad.a
LIBIBVERBS?=	${DESTDIR}${LIBDIR_BASE}/libibverbs.a
LIBLN?=		"don't use LIBLN, use LIBL"
LIBLZMA?=	${DESTDIR}${LIBDIR_BASE}/liblzma.a
LIBM?=		${DESTDIR}${LIBDIR_BASE}/libm.a
LIBMAGIC?=	${DESTDIR}${LIBDIR_BASE}/libmagic.a
LIBMD?=		${DESTDIR}${LIBDIR_BASE}/libmd.a
LIBMEMSTAT?=	${DESTDIR}${LIBDIR_BASE}/libmemstat.a
LIBMENU?=	${DESTDIR}${LIBDIR_BASE}/libmenu.a
LIBMILTER?=	${DESTDIR}${LIBDIR_BASE}/libmilter.a
LIBMLX4?=	${DESTDIR}${LIBDIR_BASE}/libmlx4.a
LIBMLX5?=	${DESTDIR}${LIBDIR_BASE}/libmlx5.a
LIBMP?=		${DESTDIR}${LIBDIR_BASE}/libmp.a
LIBMT?=		${DESTDIR}${LIBDIR_BASE}/libmt.a
LIBNANDFS?=	${DESTDIR}${LIBDIR_BASE}/libnandfs.a
LIBNCURSES?=	${DESTDIR}${LIBDIR_BASE}/libncurses.a
LIBNCURSESW?=	${DESTDIR}${LIBDIR_BASE}/libncursesw.a
LIBNETGRAPH?=	${DESTDIR}${LIBDIR_BASE}/libnetgraph.a
LIBNGATM?=	${DESTDIR}${LIBDIR_BASE}/libngatm.a
LIBNV?=		${DESTDIR}${LIBDIR_BASE}/libnv.a
LIBNVPAIR?=	${DESTDIR}${LIBDIR_BASE}/libnvpair.a
LIBOPENSM?=	${DESTDIR}${LIBDIR_BASE}/libopensm.a
LIBOPIE?=	${DESTDIR}${LIBDIR_BASE}/libopie.a
LIBOSMCOMP?=	${DESTDIR}${LIBDIR_BASE}/libosmcomp.a
LIBOSMVENDOR?=	${DESTDIR}${LIBDIR_BASE}/libosmvendor.a
LIBPAM?=	${DESTDIR}${LIBDIR_BASE}/libpam.a
LIBPANEL?=	${DESTDIR}${LIBDIR_BASE}/libpanel.a
LIBPANELW?=	${DESTDIR}${LIBDIR_BASE}/libpanelw.a
LIBPCAP?=	${DESTDIR}${LIBDIR_BASE}/libpcap.a
LIBPJDLOG?=	${DESTDIR}${LIBDIR_BASE}/libpjdlog.a
LIBPMC?=	${DESTDIR}${LIBDIR_BASE}/libpmc.a
LIBPROC?=	${DESTDIR}${LIBDIR_BASE}/libproc.a
LIBPROCSTAT?=	${DESTDIR}${LIBDIR_BASE}/libprocstat.a
LIBPTHREAD?=	${DESTDIR}${LIBDIR_BASE}/libpthread.a
LIBRADIUS?=	${DESTDIR}${LIBDIR_BASE}/libradius.a
LIBRDMACM?=	${DESTDIR}${LIBDIR_BASE}/librdmacm.a
LIBROKEN?=	${DESTDIR}${LIBDIR_BASE}/libroken.a
LIBRPCSEC_GSS?=	${DESTDIR}${LIBDIR_BASE}/librpcsec_gss.a
LIBRPCSVC?=	${DESTDIR}${LIBDIR_BASE}/librpcsvc.a
LIBRT?=		${DESTDIR}${LIBDIR_BASE}/librt.a
LIBRTLD_DB?=	${DESTDIR}${LIBDIR_BASE}/librtld_db.a
LIBSBUF?=	${DESTDIR}${LIBDIR_BASE}/libsbuf.a
LIBSDP?=	${DESTDIR}${LIBDIR_BASE}/libsdp.a
LIBSMB?=	${DESTDIR}${LIBDIR_BASE}/libsmb.a
LIBSSL?=	${DESTDIR}${LIBDIR_BASE}/libssl.a
LIBSSP_NONSHARED?=	${DESTDIR}${LIBDIR_BASE}/libssp_nonshared.a
LIBSTDCPLUSPLUS?= ${DESTDIR}${LIBDIR_BASE}/libstdc++.a
LIBSTDTHREADS?=	${DESTDIR}${LIBDIR_BASE}/libstdthreads.a
LIBSYSDECODE?=	${DESTDIR}${LIBDIR_BASE}/libsysdecode.a
LIBTACPLUS?=	${DESTDIR}${LIBDIR_BASE}/libtacplus.a
LIBTERMCAP?=	${DESTDIR}${LIBDIR_BASE}/libtermcap.a
LIBTERMCAPW?=	${DESTDIR}${LIBDIR_BASE}/libtermcapw.a
LIBTERMLIB?=	"don't use LIBTERMLIB, use LIBTERMCAP"
LIBTINFO?=	"don't use LIBTINFO, use LIBNCURSES"
LIBUFS?=	${DESTDIR}${LIBDIR_BASE}/libufs.a
LIBUGIDFW?=	${DESTDIR}${LIBDIR_BASE}/libugidfw.a
LIBULOG?=	${DESTDIR}${LIBDIR_BASE}/libulog.a
LIBUMEM?=	${DESTDIR}${LIBDIR_BASE}/libumem.a
LIBUSB?=	${DESTDIR}${LIBDIR_BASE}/libusb.a
LIBUSBHID?=	${DESTDIR}${LIBDIR_BASE}/libusbhid.a
LIBUTIL?=	${DESTDIR}${LIBDIR_BASE}/libutil.a
LIBUUTIL?=	${DESTDIR}${LIBDIR_BASE}/libuutil.a
LIBVGL?=	${DESTDIR}${LIBDIR_BASE}/libvgl.a
LIBVMMAPI?=	${DESTDIR}${LIBDIR_BASE}/libvmmapi.a
LIBWIND?=	${DESTDIR}${LIBDIR_BASE}/libwind.a
LIBWRAP?=	${DESTDIR}${LIBDIR_BASE}/libwrap.a
LIBXO?=		${DESTDIR}${LIBDIR_BASE}/libxo.a
LIBXPG4?=	${DESTDIR}${LIBDIR_BASE}/libxpg4.a
LIBY?=		${DESTDIR}${LIBDIR_BASE}/liby.a
LIBYPCLNT?=	${DESTDIR}${LIBDIR_BASE}/libypclnt.a
LIBZ?=		${DESTDIR}${LIBDIR_BASE}/libz.a
LIBZFS?=	${DESTDIR}${LIBDIR_BASE}/libzfs.a
LIBZFS_CORE?=	${DESTDIR}${LIBDIR_BASE}/libzfs_core.a
LIBZPOOL?=	${DESTDIR}${LIBDIR_BASE}/libzpool.a

# enforce the 2 -lpthread and -lc to always be the last in that exact order
.if defined(LDADD)
.if ${LDADD:M-lpthread}
LDADD:=	${LDADD:N-lpthread} -lpthread
.endif
.if ${LDADD:M-lc}
LDADD:=	${LDADD:N-lc} -lc
.endif
.endif

# Only do this for src builds.
.if defined(SRCTOP)
.if defined(_LIBRARIES) && defined(LIB) && \
    ${_LIBRARIES:M${LIB}} != ""
.if !defined(LIB${LIB:tu})
.error ${.CURDIR}: Missing value for LIB${LIB:tu} in ${_this:T}.  Likely should be: LIB${LIB:tu}?= $${DESTDIR}$${LIBDIR_BASE}/lib${LIB}.a
.endif
.endif

# Derive LIB*SRCDIR from LIB*DIR
.for lib in ${_LIBRARIES}
LIB${lib:tu}SRCDIR?=	${SRCTOP}/${LIB${lib:tu}DIR:S,^${OBJTOP}/,,}
.endfor
.endif
