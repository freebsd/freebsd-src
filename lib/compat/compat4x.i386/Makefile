# $FreeBSD$

DISTRIBUTION=	compat4x

LIBS=	\
	libc.so.4 \
	libc_r.so.4 \
	libcrypto.so.1 \
	libcrypto.so.2 \
	libdevstat.so.2 \
	libgmp.so.3 \
	libedit.so.3 \
	libfetch.so.2 \
	libmp.so.3 \
	libpam.so.1 \
	libperl.so.3 \
	libposix1e.so.2 \
	libskey.so.2 \
	libssl.so.1 \
	libssl.so.2 \
	libstdc++.so.3 \
	libusb.so.0 \
	libusbhid.so.0 \
	libutil.so.3 \
	libvgl.so.2 \
	pam_cleartext_pass_ok.so \
	pam_deny.so \
	pam_opie.so \
	pam_opieaccess.so \
	pam_permit.so \
	pam_radius.so \
	pam_skey.so \
	pam_ssh.so \
	pam_tacplus.so \
	pam_unix.so

CLEANFILES+=	${LIBS}

all:	${LIBS}

.for lib in ${LIBS}
.if exists(${.CURDIR}/${lib}.gz.uu)
${lib}:	${.CURDIR}/${lib}.gz.uu
	uudecode -p ${.CURDIR}/${lib}.gz.uu | gzip -d > ${lib}
.elif exists(${.CURDIR}/${lib}.bz2.uu)
${lib}:	${.CURDIR}/${lib}.bz2.uu
	uudecode -p ${.CURDIR}/${lib}.bz2.uu | bzip2 -d > ${lib}
.endif
.endfor

beforeinstall:
	${INSTALL} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} ${LIBS} \
		${DESTDIR}${LIBCOMPATDIR}

# Get all the fruit, even though we don't set PROG.
# XXX bsd.lib.mk has fruitflies, e.g., it fails if LIBS is empty.
.include <bsd.prog.mk>
