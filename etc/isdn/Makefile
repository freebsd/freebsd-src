# $FreeBSD: src/etc/isdn/Makefile,v 1.14.12.1 2010/12/21 17:10:29 kensmith Exp $

NO_OBJ=

SCRIPTS=	answer			\
		isdntel.sh		\
		record			\
		tell			\
		tell-record		\
		unknown_incoming

FILES=		holidays.D		\
		isdnd.rates.A		\
		isdnd.rates.D		\
		isdnd.rates.F		\
		isdnd.rates.L		\
		isdnd.rates.UK.BT	\
		isdnd.rc.sample		\
		isdntel.alias.sample

BINDIR=		/etc/isdn
BINMODE=	700
FILESMODE=	600

.include <bsd.prog.mk>
