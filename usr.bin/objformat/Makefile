# $FreeBSD$

PROG=		objformat
NOMAN=		not yet
CFLAGS+=	-DMAIN -DROOTDIR=\"${DESTDIR}/usr/libexec/\"
.if ${BINFORMAT} == elf
CFLAGS+=	-DFREEBSD_ELF
.else
CFLAGS+=	-DFREEBSD_AOUT
.endif

objformat:
	@echo Compiles on install.

install:
	$(CC) ${CFLAGS} ${.CURDIR}/objformat.c -o ${DESTDIR}/usr/bin/objformat
	chown ${BINOWN} ${DESTDIR}/usr/bin/objformat
	chgrp ${BINGRP} ${DESTDIR}/usr/bin/objformat
	chmod ${BINMODE} ${DESTDIR}/usr/bin/objformat
	ln -s ${DESTDIR}/usr/bin/objformat ${DESTDIR}/usr/bin/ar
	ln -s ${DESTDIR}/usr/bin/objformat ${DESTDIR}/usr/bin/as
	ln -s ${DESTDIR}/usr/bin/objformat ${DESTDIR}/usr/bin/ld
	ln -s ${DESTDIR}/usr/bin/objformat ${DESTDIR}/usr/bin/nm
	ln -s ${DESTDIR}/usr/bin/objformat ${DESTDIR}/usr/bin/ranlib
	ln -s ${DESTDIR}/usr/bin/objformat ${DESTDIR}/usr/bin/size
	ln -s ${DESTDIR}/usr/bin/objformat ${DESTDIR}/usr/bin/strings
	ln -s ${DESTDIR}/usr/bin/objformat ${DESTDIR}/usr/bin/strip

.include <bsd.prog.mk>
