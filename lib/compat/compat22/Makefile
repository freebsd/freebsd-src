# $FreeBSD$

DISTRIBUTION=	compat22

LIBS=	libalias.so.2.4 libc.so.3.1 libc_r.so.3.0 libcalendar.so.2.0 \
	libcom_err.so.2.0 libcurses.so.2.0 libdialog.so.3.1 libedit.so.2.0 \
	libf2c.so.2.0 libftpio.so.4.0 libg++.so.4.0 libgmp.so.3.0 \
	libgnuregex.so.2.0 libipx.so.2.0 libkvm.so.2.0 libm.so.2.0 \
	libmp.so.3.0 libmytinfo.so.2.0 libncurses.so.3.1 libopie.so.2.0 \
	libpcap.so.2.2 libreadline.so.3.0 librpcsvc.so.2.0 libscrypt.so.2.0 \
	libscsi.so.2.0 libskey.so.2.0 libss.so.2.0 libstdc++.so.2.0 \
	libtelnet.so.2.0 libtermcap.so.2.1 libutil.so.2.2 libvgl.so.1.0 \
	libxpg4.so.2.0 libz.so.2.0

CLEANFILES+=	${LIBS} ld.so

all:	${LIBS} ld.so

.for lib in ${LIBS} ld.so
${lib}:	${lib}.gz.uu
	uudecode -p ${.CURDIR}/${lib}.gz.uu | gunzip > ${lib}
.endfor

beforeinstall:
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} ld.so \
		${DESTDIR}/usr/libexec
	${INSTALL} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} ${LIBS} \
		${DESTDIR}${LIBCOMPATDIR}/aout
	@${ECHO} "libcrypt.so.2.0 -> libscrypt.so.2.0" ; \
		cd ${DESTDIR}${LIBCOMPATDIR}/aout ; \
		ln -sf libscrypt.so.2.0 libcrypt.so.2.0
	@${ECHO} "libtermlib.so.2.1 -> libtermcap.so.2.1" ; \
		cd ${DESTDIR}${LIBCOMPATDIR}/aout ; \
		ln -sf libtermcap.so.2.1 libtermlib.so.2.1

# Get all the fruit, even though we don't set PROG.
# XXX bsd.lib.mk has fruitflies, e.g., it fails if LIBS is empty.
.include <bsd.prog.mk>
