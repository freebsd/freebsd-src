#
# $FreeBSD: src/usr.sbin/xntpd/parse/Makefile,v 1.3.2.1 1999/08/29 15:49:28 peter Exp $
#

PARSEFLAGS= -DCLOCK_SCHMID -DCLOCK_DCF7000 -DCLOCK_MEINBERG \
	-DCLOCK_RAWDCF -DCLOCK_TRIMSV6

CFLAGS+= -I${.CURDIR}/../include ${PARSEFLAGS}

SRCS= parse.c parse_conf.c clk_meinberg.c clk_schmid.c clk_rawdcf.c \
	clk_dcf7000.c clk_trimble.c

NOMAN=
NOPROFILE=
LIB= parse

install:

.include <bsd.lib.mk>
