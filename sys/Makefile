# $FreeBSD$

# The boot loader
.if ${MACHINE_ARCH} != "arm"
SUBDIR=	boot
.endif

# Loadable kernel modules
.if defined(MODULES_WITH_WORLD)
SUBDIR+=modules
.endif

HTAGSFLAGS+= -at `awk -F= '/^RELEASE *=/{release=$2}; END {print "FreeBSD", release, "kernel"}' < conf/newvers.sh`

.include <bsd.subdir.mk>
