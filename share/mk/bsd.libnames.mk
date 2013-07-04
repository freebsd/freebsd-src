# $FreeBSD$

# The include file <bsd.libnames.mk> define library names.
# Other include files (e.g. bsd.prog.mk, bsd.lib.mk) include this
# file where necessary.

.if !target(__<bsd.init.mk>__)
.error bsd.libnames.mk cannot be included directly.
.endif

LIBCRT0?=	${DESTDIR}${LIBDIR}/crt0.o

LIBALIAS?=	${DESTDIR}${LIBDIR}/libalias.a
LIBARCHIVE?=	${DESTDIR}${LIBDIR}/libarchive.a
LIBASN1?=	${DESTDIR}${LIBDIR}/libasn1.a
LIBATF_C?=	${DESTDIR}${LIBDIR}/libatf-c.a
LIBATF_CXX?=	${DESTDIR}${LIBDIR}/libatf-c++.a
LIBATM?=	${DESTDIR}${LIBDIR}/libatm.a
LIBAUDITD?=	${DESTDIR}${LIBDIR}/libauditd.a
LIBAVL?=	${DESTDIR}${LIBDIR}/libavl.a
LIBBEGEMOT?=	${DESTDIR}${LIBDIR}/libbegemot.a
.if ${MK_BIND_LIBS} != "no"
LIBBIND?=	${DESTDIR}${LIBDIR}/libbind.a
LIBBIND9?=	${DESTDIR}${LIBDIR}/libbind9.a
.endif
LIBBLUETOOTH?=	${DESTDIR}${LIBDIR}/libbluetooth.a
LIBBSDXML?=	${DESTDIR}${LIBDIR}/libbsdxml.a
LIBBSDYML?=	${DESTDIR}${LIBDIR}/libbsdyml.a
LIBBSM?=	${DESTDIR}${LIBDIR}/libbsm.a
LIBBSNMP?=	${DESTDIR}${LIBDIR}/libbsnmp.a
LIBBZ2?=	${DESTDIR}${LIBDIR}/libbz2.a
.if ${MK_LIBCPLUSPLUS} != "no"
LIBCXXRT?=	${DESTDIR}${LIBDIR}/libcxxrt.a
LIBCPLUSPLUS?=	${DESTDIR}${LIBDIR}/libc++.a
.endif
LIBC?=		${DESTDIR}${LIBDIR}/libc.a
LIBC_PIC?=	${DESTDIR}${LIBDIR}/libc_pic.a
LIBCALENDAR?=	${DESTDIR}${LIBDIR}/libcalendar.a
LIBCAM?=	${DESTDIR}${LIBDIR}/libcam.a
LIBCOM_ERR?=	${DESTDIR}${LIBDIR}/libcom_err.a
LIBCOMPAT?=	${DESTDIR}${LIBDIR}/libcompat.a
LIBCRYPT?=	${DESTDIR}${LIBDIR}/libcrypt.a
LIBCRYPTO?=	${DESTDIR}${LIBDIR}/libcrypto.a
LIBCTF?=	${DESTDIR}${LIBDIR}/libctf.a
LIBCURSES?=	${DESTDIR}${LIBDIR}/libcurses.a
LIBDEVINFO?=	${DESTDIR}${LIBDIR}/libdevinfo.a
LIBDEVSTAT?=	${DESTDIR}${LIBDIR}/libdevstat.a
LIBDIALOG?=	${DESTDIR}${LIBDIR}/libdialog.a
LIBDNS?=	${DESTDIR}${LIBDIR}/libdns.a
LIBDTRACE?=	${DESTDIR}${LIBDIR}/libdtrace.a
LIBDWARF?=	${DESTDIR}${LIBDIR}/libdwarf.a
LIBEDIT?=	${DESTDIR}${LIBDIR}/libedit.a
LIBELF?=	${DESTDIR}${LIBDIR}/libelf.a
LIBFETCH?=	${DESTDIR}${LIBDIR}/libfetch.a
LIBFL?=		"don't use LIBFL, use LIBL"
LIBFORM?=	${DESTDIR}${LIBDIR}/libform.a
LIBG2C?=	${DESTDIR}${LIBDIR}/libg2c.a
LIBGCC?=	${DESTDIR}${LIBDIR}/libgcc.a
LIBGCC_PIC?=	${DESTDIR}${LIBDIR}/libgcc_pic.a
LIBGEOM?=	${DESTDIR}${LIBDIR}/libgeom.a
LIBGNUREGEX?=	${DESTDIR}${LIBDIR}/libgnuregex.a
LIBGSSAPI?=	${DESTDIR}${LIBDIR}/libgssapi.a
LIBGSSAPI_KRB5?= ${DESTDIR}${LIBDIR}/libgssapi_krb5.a
LIBHDB?=	${DESTDIR}${LIBDIR}/libhdb.a
LIBHISTORY?=	${DESTDIR}${LIBDIR}/libhistory.a
LIBHEIMBASE?=	${DESTDIR}${LIBDIR}/libheimbase.a
LIBHEIMNTLM?=	${DESTDIR}${LIBDIR}/libheimntlm.a
LIBHEIMSQLITE?=	${DESTDIR}${LIBDIR}/libheimsqlite.a
LIBHX509?=	${DESTDIR}${LIBDIR}/libhx509.a
LIBIPSEC?=	${DESTDIR}${LIBDIR}/libipsec.a
.if ${MK_IPX} != "no"
LIBIPX?=	${DESTDIR}${LIBDIR}/libipx.a
.endif
.if ${MK_BIND_LIBS} != "no"
LIBISC?=	${DESTDIR}${LIBDIR}/libisc.a
LIBISCCC?=	${DESTDIR}${LIBDIR}/libisccc.a
LIBISCCFG?=	${DESTDIR}${LIBDIR}/libisccfg.a
.endif
LIBJAIL?=	${DESTDIR}${LIBDIR}/libjail.a
LIBKADM5CLNT?=	${DESTDIR}${LIBDIR}/libkadm5clnt.a
LIBKADM5SRV?=	${DESTDIR}${LIBDIR}/libkadm5srv.a
LIBKAFS5?=	${DESTDIR}${LIBDIR}/libkafs5.a
LIBKDC?=	${DESTDIR}${LIBDIR}/libkdc.a
LIBKEYCAP?=	${DESTDIR}${LIBDIR}/libkeycap.a
LIBKICONV?=	${DESTDIR}${LIBDIR}/libkiconv.a
LIBKRB5?=	${DESTDIR}${LIBDIR}/libkrb5.a
LIBKVM?=	${DESTDIR}${LIBDIR}/libkvm.a
LIBL?=		${DESTDIR}${LIBDIR}/libl.a
.if ${MK_LDNS} != "no"
LIBLDNS?=	${DESTDIR}${LIBDIR}/libldns.a
.endif
LIBLN?=		"don't use LIBLN, use LIBL"
.if ${MK_BIND} != "no"
LIBLWRES?=	${DESTDIR}${LIBDIR}/liblwres.a
.endif
LIBLZMA?=	${DESTDIR}${LIBDIR}/liblzma.a
LIBM?=		${DESTDIR}${LIBDIR}/libm.a
LIBMAGIC?=	${DESTDIR}${LIBDIR}/libmagic.a
LIBMD?=		${DESTDIR}${LIBDIR}/libmd.a
LIBMEMSTAT?=	${DESTDIR}${LIBDIR}/libmemstat.a
LIBMENU?=	${DESTDIR}${LIBDIR}/libmenu.a
.if ${MK_SENDMAIL} != "no"
LIBMILTER?=	${DESTDIR}${LIBDIR}/libmilter.a
.endif
LIBMP?=		${DESTDIR}${LIBDIR}/libmp.a
LIBNCURSES?=	${DESTDIR}${LIBDIR}/libncurses.a
LIBNCURSESW?=	${DESTDIR}${LIBDIR}/libncursesw.a
LIBNETGRAPH?=	${DESTDIR}${LIBDIR}/libnetgraph.a
LIBNGATM?=	${DESTDIR}${LIBDIR}/libngatm.a
LIBNVPAIR?=	${DESTDIR}${LIBDIR}/libnvpair.a
LIBOPIE?=	${DESTDIR}${LIBDIR}/libopie.a

# The static PAM library doesn't know its secondary dependencies,
# so we have to specify them explicitly.
LIBPAM?=	${DESTDIR}${LIBDIR}/libpam.a
MINUSLPAM=	-lpam
.if defined(LDFLAGS) && !empty(LDFLAGS:M-static)
.if ${MK_KERBEROS} != "no"
LIBPAM+=	${LIBKRB5} ${LIBHX509} ${LIBASN1} ${LIBCRYPTO} ${LIBCRYPT} \
		${LIBROKEN} ${LIBCOM_ERR}
MINUSLPAM+=	-lkrb5 -lhx509 -lasn1 -lcrypto -lcrypt -lroken -lcom_err
.endif
LIBPAM+=	${LIBRADIUS} ${LIBTACPLUS} ${LIBCRYPT} \
		${LIBUTIL} ${LIBOPIE} ${LIBMD}
MINUSLPAM+=	-lradius -ltacplus -lcrypt \
		-lutil -lopie -lmd
.if ${MK_OPENSSH} != "no"
LIBPAM+=	${LIBSSH} ${LIBCRYPTO} ${LIBCRYPT}
MINUSLPAM+=	-lssh -lcrypto -lcrypt
.endif
.if ${MK_NIS} != "no"
LIBPAM+=	${LIBYPCLNT}
MINUSLPAM+=	-lypclnt
.endif
.endif

LIBPANEL?=	${DESTDIR}${LIBDIR}/libpanel.a
LIBPCAP?=	${DESTDIR}${LIBDIR}/libpcap.a
LIBPMC?=	${DESTDIR}${LIBDIR}/libpmc.a
LIBPROC?=	${DESTDIR}${LIBDIR}/libproc.a
LIBPROCSTAT?=	${DESTDIR}${LIBDIR}/libprocstat.a
LIBPTHREAD?=	${DESTDIR}${LIBDIR}/libpthread.a
LIBRADIUS?=	${DESTDIR}${LIBDIR}/libradius.a
LIBREADLINE?=	${DESTDIR}${LIBDIR}/libreadline.a
LIBROKEN?=	${DESTDIR}${LIBDIR}/libroken.a
LIBRPCSVC?=	${DESTDIR}${LIBDIR}/librpcsvc.a
LIBRPCSEC_GSS?=	${DESTDIR}${LIBDIR}/librpcsec_gss.a
LIBRT?=		${DESTDIR}${LIBDIR}/librt.a
LIBRTLD_DB?=	${DESTDIR}${LIBDIR}/librtld_db.a
LIBSBUF?=	${DESTDIR}${LIBDIR}/libsbuf.a
LIBSDP?=	${DESTDIR}${LIBDIR}/libsdp.a
LIBSMB?=	${DESTDIR}${LIBDIR}/libsmb.a
LIBSSH?=	${DESTDIR}${LIBDIR}/libssh.a
LIBSSL?=	${DESTDIR}${LIBDIR}/libssl.a
LIBSTAND?=	${DESTDIR}${LIBDIR}/libstand.a
LIBSTDCPLUSPLUS?= ${DESTDIR}${LIBDIR}/libstdc++.a
LIBTACPLUS?=	${DESTDIR}${LIBDIR}/libtacplus.a
LIBTERMCAP?=	${DESTDIR}${LIBDIR}/libtermcap.a
LIBTERMLIB?=	"don't use LIBTERMLIB, use LIBTERMCAP"
LIBTINFO?=	"don't use LIBTINFO, use LIBNCURSES"
LIBUFS?=	${DESTDIR}${LIBDIR}/libufs.a
LIBUGIDFW?=	${DESTDIR}${LIBDIR}/libugidfw.a
LIBUMEM?=	${DESTDIR}${LIBDIR}/libumem.a
LIBUSBHID?=	${DESTDIR}${LIBDIR}/libusbhid.a
LIBUSB?=	${DESTDIR}${LIBDIR}/libusb.a
LIBULOG?=	${DESTDIR}${LIBDIR}/libulog.a
LIBUTIL?=	${DESTDIR}${LIBDIR}/libutil.a
LIBUUTIL?=	${DESTDIR}${LIBDIR}/libuutil.a
LIBVGL?=	${DESTDIR}${LIBDIR}/libvgl.a
LIBVMMAPI?=	${DESTDIR}${LIBDIR}/libvmmapi.a
LIBWIND?=	${DESTDIR}${LIBDIR}/libwind.a
LIBWRAP?=	${DESTDIR}${LIBDIR}/libwrap.a
LIBXPG4?=	${DESTDIR}${LIBDIR}/libxpg4.a
LIBY?=		${DESTDIR}${LIBDIR}/liby.a
LIBYPCLNT?=	${DESTDIR}${LIBDIR}/libypclnt.a
LIBZ?=		${DESTDIR}${LIBDIR}/libz.a
LIBZFS?=	${DESTDIR}${LIBDIR}/libzfs.a
LIBZFS_CORE?=	${DESTDIR}${LIBDIR}/libzfs_core.a
LIBZPOOL?=	${DESTDIR}${LIBDIR}/libzpool.a
