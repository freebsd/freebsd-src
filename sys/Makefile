# $FreeBSD$

# The boot loader
SUBDIR=	boot

# KLD modules build for both a.out and ELF
.if defined(MODULES_WITH_WORLD)
SUBDIR+=modules
.endif

afterdistribute:
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m 444 \
		${.CURDIR}/${MACHINE_ARCH}/conf/GENERIC.hints \
		${DESTDIR}/boot/device.hints

HTAGSFLAGS+= -at `awk -F= '/^RELEASE *=/{release=$2}; END {print "FreeBSD", release, "kernel"}' < conf/newvers.sh`

.include <bsd.subdir.mk>
