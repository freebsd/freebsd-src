#	$Id: bsd.libnames.mk,v 1.3.2.2 1997/05/24 10:27:06 brian Exp $
#
# The include file <bsd.libnames.mk> define library names. 
# Other include files (e.g. bsd.prog.mk, bsd.lib.mk) include this 
# file where necessary.


LIBCRT0?=	${DESTDIR}/usr/lib/crt0.o
LIBKZHEAD?=	${DESTDIR}/usr/lib/kzhead.o
LIBKZTAIL?=	${DESTDIR}/usr/lib/kztail.o

LIBALIAS?=	${DESTDIR}/usr/lib/libalias.a
LIBC?=		${DESTDIR}/usr/lib/libc.a
LIBC_PIC=	${DESTDIR}/usr/lib/libc_pic.a
LIBCALENDAR?=	${DESTDIR}/usr/lib/libcalendar.a
LIBCOM_ERR=	${DESTDIR}/usr/lib/libcom_err.a
LIBCOMPAT?=	${DESTDIR}/usr/lib/libcompat.a
LIBCRYPT?=	${DESTDIR}/usr/lib/libcrypt.a
LIBCURSES?=	${DESTDIR}/usr/lib/libcurses.a
LIBDES?=	${DESTDIR}/usr/lib/libdes.a	# XXX doesn't exist
LIBDIALOG?=	${DESTDIR}/usr/lib/libdialog.a
LIBDISK?=	${DESTDIR}/usr/lib/libdisk.a
LIBEDIT?=	${DESTDIR}/usr/lib/libedit.a
LIBF2C?=	${DESTDIR}/usr/lib/libf2c.a
LIBFL?=		"don't use LIBFL, use LIBL"
LIBFORMS?=	${DESTDIR}/usr/lib/libforms.a
LIBFTPIO?=	${DESTDIR}/usr/lib/libftpio.a
LIBGPLUSPLUS?=	${DESTDIR}/usr/lib/libg++.a
LIBGCC?=	${DESTDIR}/usr/lib/libgcc.a
LIBGCC_PIC?=	${DESTDIR}/usr/lib/libgcc_pic.a
LIBGMP?=	${DESTDIR}/usr/lib/libgmp.a
LIBGNUREGEX?=	${DESTDIR}/usr/lib/libgnuregex.a
LIBIPX?=	${DESTDIR}/usr/lib/libipx.a
LIBKDB?=	${DESTDIR}/usr/lib/libkdb.a	# XXX doesn't exist
LIBKRB?=	${DESTDIR}/usr/lib/libkrb.a	# XXX doesn't exist
LIBKEYCAP?=	${DESTDIR}/usr/lib/libkeycap.a
LIBKVM?=	${DESTDIR}/usr/lib/libkvm.a
LIBL?=		${DESTDIR}/usr/lib/libl.a
LIBLN?=		"don't use, LIBLN, use LIBL"
LIBM?=		${DESTDIR}/usr/lib/libm.a
LIBMD?=		${DESTDIR}/usr/lib/libmd.a
LIBMP?=		${DESTDIR}/usr/lib/libmp.a
LIBMYTINFO?=	${DESTDIR}/usr/lib/libmytinfo.a
LIBNCURSES?=	${DESTDIR}/usr/lib/libncurses.a
LIBOPIE?=	${DESTDIR}/usr/lib/libopie.a
LIBPC?=		${DESTDIR}/usr/lib/libpc.a	# XXX doesn't exist
LIBPCAP?=	${DESTDIR}/usr/lib/libpcap.a
LIBPLOT?=	${DESTDIR}/usr/lib/libplot.a	# XXX doesn't exist
LIBREADLINE?=	${DESTDIR}/usr/lib/libreadline.a
LIBRESOLV?=	${DESTDIR}/usr/lib/libresolv.a	# XXX doesn't exist
LIBRPCSVC?=	${DESTDIR}/usr/lib/librpcsvc.a
LIBSCRYPT?=	"don't use LIBSCRYPT, use LIBCRYPT"
LIBSCSI?=	${DESTDIR}/usr/lib/libscsi.a
LIBSKEY?=	${DESTDIR}/usr/lib/libskey.a
LIBSS?=		${DESTDIR}/usr/lib/libss.a
LIBSTDCPLUSPLUS?= ${DESTDIR}/usr/lib/libstdc++.a
LIBTCL?=	${DESTDIR}/usr/lib/libtcl.a
LIBTELNET?=	${DESTDIR}/usr/lib/libtelnet.a
LIBTERMCAP?=	${DESTDIR}/usr/lib/libtermcap.a
LIBTERMLIB?=	"don't use LIBTERMLIB, use LIBTERMCAP"
LIBUTIL?=	${DESTDIR}/usr/lib/libutil.a
LIBXPG4?=	${DESTDIR}/usr/lib/libxpg4.a
LIBY?=		${DESTDIR}/usr/lib/liby.a
LIBZ?=		${DESTDIR}/usr/lib/libz.a
