# $FreeBSD: src/sys/Makefile,v 1.30.2.1 2004/08/22 14:19:53 des Exp $

# The boot loader
.if !defined(NO_BOOT)
.if ${MACHINE_ARCH} != "arm"
SUBDIR=	boot
.endif
.endif

# Loadable kernel modules
.if defined(MODULES_WITH_WORLD)
SUBDIR+=modules
.endif

HTAGSFLAGS+= -at `awk -F= '/^RELEASE *=/{release=$2}; END {print "FreeBSD", release, "kernel"}' < conf/newvers.sh`

.include <bsd.subdir.mk>
