# $FreeBSD$

# The include file <bsd.libnames.mk> define library names. 
# Other include files (e.g. bsd.prog.mk, bsd.lib.mk) include this 
# file where necessary.

.if !target(__<bsd.init.mk>__)
.error bsd.libnames.mk cannot be included directly.
.endif

LIBCRT0?=	${DESTDIR}${LIBDIR}/crt0.o

LIBALIAS?=	${DESTDIR}${LIBDIR}/libalias.a
LIBASN1?=	${DESTDIR}${LIBDIR}/libasn1.a	# XXX in secure dist, not base
LIBATM?=	${DESTDIR}${LIBDIR}/libatm.a
LIBBSDXML?=	${DESTDIR}${LIBDIR}/libbsdxml.a
LIBBLUETOOTH?=	${DESTDIR}${LIBDIR}/libbluetooth.a
LIBBZ2?=	${DESTDIR}${LIBDIR}/libbz2.a
LIBC?=		${DESTDIR}${LIBDIR}/libc.a
LIBC_PIC?=	${DESTDIR}${LIBDIR}/libc_pic.a
LIBC_R?=	${DESTDIR}${LIBDIR}/libc_r.a
LIBCALENDAR?=	${DESTDIR}${LIBDIR}/libcalendar.a
LIBCAM?=	${DESTDIR}${LIBDIR}/libcam.a
LIBCOM_ERR?=	${DESTDIR}${LIBDIR}/libcom_err.a
LIBCOMPAT?=	${DESTDIR}${LIBDIR}/libcompat.a
LIBCRYPT?=	${DESTDIR}${LIBDIR}/libcrypt.a
LIBCRYPTO?=	${DESTDIR}${LIBDIR}/libcrypto.a	# XXX in secure dist, not base
LIBCURSES?=	${DESTDIR}${LIBDIR}/libcurses.a
LIBDES?=	${DESTDIR}${LIBDIR}/libdes.a	# XXX in secure dist, not base
LIBDEVINFO?=	${DESTDIR}${LIBDIR}/libdevinfo.a
LIBDEVSTAT?=	${DESTDIR}${LIBDIR}/libdevstat.a
LIBDIALOG?=	${DESTDIR}${LIBDIR}/libdialog.a
LIBDISK?=	${DESTDIR}${LIBDIR}/libdisk.a
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
LIBGSSAPI?=	${DESTDIR}${LIBDIR}/libgssapi.a	# XXX in secure dist, not base
LIBHDB?=	${DESTDIR}${LIBDIR}/libhdb.a	# XXX in secure dist, not base
LIBHISTORY?=	${DESTDIR}${LIBDIR}/libhistory.a
LIBIPSEC?=	${DESTDIR}${LIBDIR}/libipsec.a
LIBIPX?=	${DESTDIR}${LIBDIR}/libipx.a
LIBISC?=	${DESTDIR}${LIBDIR}/libisc.a
LIBKADM5CLNT?=	${DESTDIR}${LIBDIR}/libkadm5clnt.a # XXX in secure dist, not base
LIBKADM5SRV?=	${DESTDIR}${LIBDIR}/libkadm5srv.a # XXX in secure dist, not base
LIBKAFS5?=	${DESTDIR}${LIBDIR}/libkafs5.a # XXX in secure dist, not base
LIBKEYCAP?=	${DESTDIR}${LIBDIR}/libkeycap.a
LIBKICONV?=	${DESTDIR}${LIBDIR}/libkiconv.a
LIBKRB5?=	${DESTDIR}${LIBDIR}/libkrb5.a	# XXX in secure dist, not base
LIBKVM?=	${DESTDIR}${LIBDIR}/libkvm.a
LIBL?=		${DESTDIR}${LIBDIR}/libl.a
LIBLN?=		"don't use LIBLN, use LIBL"
LIBM?=		${DESTDIR}${LIBDIR}/libm.a
LIBMD?=		${DESTDIR}${LIBDIR}/libmd.a
LIBMENU?=	${DESTDIR}${LIBDIR}/libmenu.a
.if !defined(NO_SENDMAIL)
LIBMILTER?=	${DESTDIR}${LIBDIR}/libmilter.a
.endif
LIBMP?=		${DESTDIR}${LIBDIR}/libmp.a
LIBMYTINFO?=	"don't use LIBMYTINFO, use LIBNCURSES"
LIBNCP?=	${DESTDIR}${LIBDIR}/libncp.a
LIBNCURSES?=	${DESTDIR}${LIBDIR}/libncurses.a
LIBNETGRAPH?=	${DESTDIR}${LIBDIR}/libnetgraph.a
LIBOBJC?=	${DESTDIR}${LIBDIR}/libobjc.a
LIBOPIE?=	${DESTDIR}${LIBDIR}/libopie.a

# The static PAM library doesn't know its secondary dependencies,
# so we have to specify them explicitly.
LIBPAM?=	${DESTDIR}${LIBDIR}/libpam.a
MINUSLPAM?=	-lpam
.if defined(LDFLAGS) && !empty(LDFLAGS:M-static)
.if !defined(NO_KERBEROS)
LIBPAM+=	${LIBKRB5} ${LIBASN1} ${LIBROKEN}
MINUSLPAM+=	-lkrb5 -lasn1 -lroken
LIBPAM+=	${LIBCOM_ERR}
MINUSLPAM+=	-lcom_err
.endif
LIBPAM+=	${LIBRADIUS} ${LIBRPCSVC} ${LIBTACPLUS} ${LIBCRYPT} \
		${LIBUTIL} ${LIBOPIE} ${LIBMD} ${LIBYPCLNT}
MINUSLPAM+=	-lradius -lrpcsvc -ltacplus -lcrypt \
		-lutil -lopie -lmd -lypclnt
.if !defined(NOCRYPT) && !defined(NO_OPENSSL) && !defined(NO_OPENSSH)
LIBPAM+=	${LIBSSH} ${LIBCRYPTO}
MINUSLPAM+=	-lssh -lcrypto
.endif
.endif

LIBPANEL?=	${DESTDIR}${LIBDIR}/libpanel.a
LIBPC?=		${DESTDIR}${LIBDIR}/libpc.a	# XXX doesn't exist
LIBPCAP?=	${DESTDIR}${LIBDIR}/libpcap.a
LIBPERL?=	${DESTDIR}${LIBDIR}/libperl.a
LIBPLOT?=	${DESTDIR}${LIBDIR}/libplot.a	# XXX doesn't exist
LIBRADIUS?=	${DESTDIR}${LIBDIR}/libradius.a
LIBREADLINE?=	${DESTDIR}${LIBDIR}/libreadline.a
LIBRESOLV?=	${DESTDIR}${LIBDIR}/libresolv.a	# XXX doesn't exist
LIBROKEN?=	${DESTDIR}${LIBDIR}/libroken.a	# XXX in secure dist, not base
LIBRPCSVC?=	${DESTDIR}${LIBDIR}/librpcsvc.a
LIBSDP?=	${DESTDIR}${LIBDIR}/libsdp.a
LIBSBUF?=	${DESTDIR}${LIBDIR}/libsbuf.a
LIBSMB?=	${DESTDIR}${LIBDIR}/libsmb.a
LIBSSH?=	${DESTDIR}${LIBDIR}/libssh.a	# XXX in secure dist, not base
LIBSSL?=	${DESTDIR}${LIBDIR}/libssl.a	# XXX in secure dist, not base
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
