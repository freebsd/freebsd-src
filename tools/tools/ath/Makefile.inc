#	$FreeBSD$

BINDIR=	/usr/local/bin
NO_MAN=

ATH_DEFAULT=	ath0

CFLAGS+=-DATH_DEFAULT='"${ATH_DEFAULT}"'
CFLAGS+=-I../common
CFLAGS+=-I../../../../sys/dev/ath
CFLAGS+=-I../../../../sys/dev/ath/ath_hal
