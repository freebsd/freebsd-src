# $FreeBSD$

# The include file <bsd.libnames.mk> define library names. 
# Other include files (e.g. bsd.prog.mk, bsd.lib.mk) include this 
# file where necessary.

LIBCRT0?=	${DESTDIR}${LIBDIR}/crt0.o

LIBALIAS?=	${DESTDIR}${LIBDIR}/libalias.a
LIBASN1?=	${DESTDIR}${LIBDIR}/libasn1.a	# XXX in secure dist, not base
LIBATM?=	${DESTDIR}${LIBDIR}/libatm.a
LIBBZ2?=	${DESTDIR}${LIBDIR}/libbz2.a
LIBC?=		${DESTDIR}${LIBDIR}/libc.a
LIBC_PIC?=	${DESTDIR}${LIBDIR}/libc_pic.a
LIBC_R?=	${DESTDIR}${LIBDIR}/libc_r.a
LIBCALENDAR?=	${DESTDIR}${LIBDIR}/libcalendar.a
LIBCAM?=	${DESTDIR}${LIBDIR}/libcam.a
LIBCIPHER?=	${DESTDIR}${LIBDIR}/libcipher.a	# XXX in secure dist, not base
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
LIBGNUREGEX?=	${DESTDIR}${LIBDIR}/libgnuregex.a
LIBHISTORY?=	${DESTDIR}${LIBDIR}/libhistory.a
LIBIPSEC?=	${DESTDIR}${LIBDIR}/libipsec.a
LIBIPX?=	${DESTDIR}${LIBDIR}/libipx.a
LIBISC?=	${DESTDIR}${LIBDIR}/libisc.a
LIBKDB?=	${DESTDIR}${LIBDIR}/libkdb.a	# XXX in secure dist, not base
LIBKRB?=	${DESTDIR}${LIBDIR}/libkrb.a	# XXX in secure dist, not base
LIBKRB5?=	${DESTDIR}${LIBDIR}/libkrb5.a	# XXX in secure dist, not base
LIBKEYCAP?=	${DESTDIR}${LIBDIR}/libkeycap.a
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
# so we have to specify them explictly.
LIBPAM?=	${DESTDIR}${LIBDIR}/libpam.a
MINUSLPAM?=	-lpam
.if defined(NOSHARED) && ${NOSHARED} != "no" && ${NOSHARED} != "NO"
.if defined(MAKE_KERBEROS4) || defined(MAKE_KERBEROS5)
.ifdef MAKE_KERBEROS4
LIBPAM+=	${LIBKRB}
MINUSLPAM+=	-lkrb
.endif
.ifdef MAKE_KERBEROS5
LIBPAM+=	${LIBKRB5} ${LIBASN1} ${LIBROKEN}
MINUSLPAM+=	-lkrb5 -lasn1 -lroken
.endif
LIBPAM+=	${LIBCOM_ERR}
MINUSLPAM+=	-lcom_err
.endif
LIBPAM+=	${LIBRADIUS} ${LIBRPCSVC} ${LIBTACPLUS} ${LIBCRYPT} \
		${LIBUTIL} ${LIBOPIE} ${LIBMD}
MINUSLPAM+=	-lradius -lrpcsvc -ltacplus -lcrypt \
		-lutil -lopie -lmd
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
LIBSBUF?=	${DESTDIR}${LIBDIR}/libsbuf.a
LIBSMB?=	${DESTDIR}${LIBDIR}/libsmb.a
LIBSSH?=	${DESTDIR}${LIBDIR}/libssh.a	# XXX in secure dist, not base
LIBSSL?=	${DESTDIR}${LIBDIR}/libssl.a	# XXX in secure dist, not base
LIBSTDCPLUSPLUS?= ${DESTDIR}${LIBDIR}/libstdc++.a
LIBTACPLUS?=	${DESTDIR}${LIBDIR}/libtacplus.a
LIBTERMCAP?=	${DESTDIR}${LIBDIR}/libtermcap.a
LIBTERMLIB?=	"don't use LIBTERMLIB, use LIBTERMCAP"
LIBTINFO?=	"don't use LIBTINFO, use LIBNCURSES"
LIBUTIL?=	${DESTDIR}${LIBDIR}/libutil.a
LIBUSB?=	${DESTDIR}${LIBDIR}/libusb.a
LIBVGL?=	${DESTDIR}${LIBDIR}/libvgl.a
LIBWRAP?=	${DESTDIR}${LIBDIR}/libwrap.a
LIBXPG4?=	${DESTDIR}${LIBDIR}/libxpg4.a
LIBY?=		${DESTDIR}${LIBDIR}/liby.a
LIBZ?=		${DESTDIR}${LIBDIR}/libz.a
