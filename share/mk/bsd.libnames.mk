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
LIBATM?=	${DESTDIR}${LIBDIR}/libatm.a
LIBBEGEMOT?=	${DESTDIR}${LIBDIR}/libbegemot.a
.if ${MK_BIND_LIBS} != "no"
LIBBIND?=	${DESTDIR}${LIBDIR}/libbind.a
LIBBIND9?=	${DESTDIR}${LIBDIR}/libbind9.a
.endif
LIBBLUETOOTH?=	${DESTDIR}${LIBDIR}/libbluetooth.a
LIBBSDXML?=	${DESTDIR}${LIBDIR}/libbsdxml.a
LIBBSM?=	${DESTDIR}${LIBDIR}/libbsm.a
LIBBSNMP?=	${DESTDIR}${LIBDIR}/libbsnmp.a
LIBBZ2?=	${DESTDIR}${LIBDIR}/libbz2.a
LIBC?=		${DESTDIR}${LIBDIR}/libc.a
LIBC_PIC?=	${DESTDIR}${LIBDIR}/libc_pic.a
LIBCALENDAR?=	${DESTDIR}${LIBDIR}/libcalendar.a
LIBCAM?=	${DESTDIR}${LIBDIR}/libcam.a
LIBCOM_ERR?=	${DESTDIR}${LIBDIR}/libcom_err.a
LIBCOMPAT?=	${DESTDIR}${LIBDIR}/libcompat.a
LIBCRYPT?=	${DESTDIR}${LIBDIR}/libcrypt.a
LIBCRYPTO?=	${DESTDIR}${LIBDIR}/libcrypto.a
LIBCURSES?=	${DESTDIR}${LIBDIR}/libcurses.a
LIBDEVINFO?=	${DESTDIR}${LIBDIR}/libdevinfo.a
LIBDEVSTAT?=	${DESTDIR}${LIBDIR}/libdevstat.a
LIBDIALOG?=	${DESTDIR}${LIBDIR}/libdialog.a
LIBDISK?=	${DESTDIR}${LIBDIR}/libdisk.a
LIBDNS?=	${DESTDIR}${LIBDIR}/libdns.a
LIBEDIT?=	${DESTDIR}${LIBDIR}/libedit.a
LIBFETCH?=	${DESTDIR}${LIBDIR}/libfetch.a
LIBFL?=		"don't use LIBFL, use LIBL"
LIBFORM?=	${DESTDIR}${LIBDIR}/libform.a
LIBFTPIO?=	${DESTDIR}${LIBDIR}/libftpio.a
LIBG2C?=	${DESTDIR}${LIBDIR}/libg2c.a
LIBGCC?=	${DESTDIR}${LIBDIR}/libgcc.a
LIBGCC_PIC?=	${DESTDIR}${LIBDIR}/libgcc_pic.a
LIBGEOM?=	${DESTDIR}${LIBDIR}/libgeom.a
LIBGNUREGEX?=	${DESTDIR}${LIBDIR}/libgnuregex.a
LIBGSSAPI?=	${DESTDIR}${LIBDIR}/libgssapi.a
LIBHDB?=	${DESTDIR}${LIBDIR}/libhdb.a
LIBHISTORY?=	${DESTDIR}${LIBDIR}/libhistory.a
LIBIPSEC?=	${DESTDIR}${LIBDIR}/libipsec.a
.if ${MK_IPX} != "no"
LIBIPX?=	${DESTDIR}${LIBDIR}/libipx.a
.endif
.if ${MK_BIND_LIBS} != "no"
LIBISC?=	${DESTDIR}${LIBDIR}/libisc.a
LIBISCCC?=	${DESTDIR}${LIBDIR}/libisccc.a
LIBISCCFG?=	${DESTDIR}${LIBDIR}/libisccfg.a
.endif
LIBKADM5CLNT?=	${DESTDIR}${LIBDIR}/libkadm5clnt.a
LIBKADM5SRV?=	${DESTDIR}${LIBDIR}/libkadm5srv.a
LIBKAFS5?=	${DESTDIR}${LIBDIR}/libkafs5.a
LIBKEYCAP?=	${DESTDIR}${LIBDIR}/libkeycap.a
LIBKICONV?=	${DESTDIR}${LIBDIR}/libkiconv.a
LIBKRB5?=	${DESTDIR}${LIBDIR}/libkrb5.a
LIBKVM?=	${DESTDIR}${LIBDIR}/libkvm.a
LIBL?=		${DESTDIR}${LIBDIR}/libl.a
LIBLN?=		"don't use LIBLN, use LIBL"
.if ${MK_BIND} != "no"
LIBLWRES?=	${DESTDIR}${LIBDIR}/liblwres.a
.endif
LIBM?=		${DESTDIR}${LIBDIR}/libm.a
LIBMAGIC?=	${DESTDIR}${LIBDIR}/libmagic.a
LIBMD?=		${DESTDIR}${LIBDIR}/libmd.a
LIBMEMSTAT?=	${DESTDIR}${LIBDIR}/libmemstat.a
LIBMENU?=	${DESTDIR}${LIBDIR}/libmenu.a
.if ${MK_SENDMAIL} != "no"
LIBMILTER?=	${DESTDIR}${LIBDIR}/libmilter.a
.endif
LIBMP?=		${DESTDIR}${LIBDIR}/libmp.a
.if ${MK_NCP} != "no"
LIBNCP?=	${DESTDIR}${LIBDIR}/libncp.a
.endif
LIBNCURSES?=	${DESTDIR}${LIBDIR}/libncurses.a
LIBNCURSESW?=	${DESTDIR}${LIBDIR}/libncursesw.a
LIBNETGRAPH?=	${DESTDIR}${LIBDIR}/libnetgraph.a
LIBNGATM?=	${DESTDIR}${LIBDIR}/libngatm.a
LIBOBJC?=	${DESTDIR}${LIBDIR}/libobjc.a
LIBOPIE?=	${DESTDIR}${LIBDIR}/libopie.a

# The static PAM library doesn't know its secondary dependencies,
# so we have to specify them explicitly.
LIBPAM?=	${DESTDIR}${LIBDIR}/libpam.a
MINUSLPAM=	-lpam
.if defined(LDFLAGS) && !empty(LDFLAGS:M-static)
.if ${MK_KERBEROS} != "no"
LIBPAM+=	${LIBKRB5} ${LIBASN1} ${LIBCRYPTO} ${LIBCRYPT} \
		${LIBROKEN} ${LIBCOM_ERR}
MINUSLPAM+=	-lkrb5 -lasn1 -lcrypto -lcrypt -lroken -lcom_err
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
LIBPTHREAD?=	${DESTDIR}${LIBDIR}/libpthread.a
LIBRADIUS?=	${DESTDIR}${LIBDIR}/libradius.a
LIBREADLINE?=	${DESTDIR}${LIBDIR}/libreadline.a
LIBROKEN?=	${DESTDIR}${LIBDIR}/libroken.a
LIBRPCSVC?=	${DESTDIR}${LIBDIR}/librpcsvc.a
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
LIBUSBHID?=	${DESTDIR}${LIBDIR}/libusbhid.a
LIBUTIL?=	${DESTDIR}${LIBDIR}/libutil.a
LIBVGL?=	${DESTDIR}${LIBDIR}/libvgl.a
LIBWRAP?=	${DESTDIR}${LIBDIR}/libwrap.a
LIBXPG4?=	${DESTDIR}${LIBDIR}/libxpg4.a
LIBY?=		${DESTDIR}${LIBDIR}/liby.a
LIBYPCLNT?=	${DESTDIR}${LIBDIR}/libypclnt.a
LIBZ?=		${DESTDIR}${LIBDIR}/libz.a
