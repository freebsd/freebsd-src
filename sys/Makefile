# $FreeBSD$

# The boot loader
SUBDIR=	boot

# KLD modules build for both a.out and ELF
SUBDIR+=modules

HTAGSFLAGS+= -at `awk -F= '/^RELEASE *=/{release=$2}; END {print "FreeBSD", release, "kernel"}' < conf/newvers.sh`

.include <bsd.subdir.mk>
