#	$FreeBSD: src/tools/tools/ath/Makefile.inc,v 1.3.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $

BINDIR=	/usr/local/bin
NO_MAN=

ATH_DEFAULT=	ath0

CFLAGS+=-DATH_DEFAULT='"${ATH_DEFAULT}"'
CFLAGS+=-I${.CURDIR}
CFLAGS+=-I${.CURDIR}/../common
CFLAGS+=-I${.CURDIR}/../../../../sys
CFLAGS+=-I${.CURDIR}/../../../../sys/dev/ath
CFLAGS+=-I${.CURDIR}/../../../../sys/dev/ath/ath_hal
