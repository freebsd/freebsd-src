# $FreeBSD: src/sys/Makefile,v 1.26 2002/04/26 17:55:21 ru Exp $

# The boot loader
SUBDIR=	boot

# KLD modules build for both a.out and ELF
.if defined(MODULES_WITH_WORLD)
SUBDIR+=modules
.endif

HTAGSFLAGS+= -at `awk -F= '/^RELEASE *=/{release=$2}; END {print "FreeBSD", release, "kernel"}' < conf/newvers.sh`

.include <bsd.subdir.mk>
