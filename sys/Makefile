# $FreeBSD: src/sys/Makefile,v 1.27 2003/05/08 06:35:39 peter Exp $

SUBDIR=
# The boot loader
.if ${MACHINE_ARCH} != "amd64"
SUBDIR+=boot
.endif

# KLD modules build for both a.out and ELF
.if defined(MODULES_WITH_WORLD)
SUBDIR+=modules
.endif

HTAGSFLAGS+= -at `awk -F= '/^RELEASE *=/{release=$2}; END {print "FreeBSD", release, "kernel"}' < conf/newvers.sh`

.include <bsd.subdir.mk>
