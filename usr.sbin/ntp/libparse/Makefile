#
# $FreeBSD$
#

.PATH:	${.CURDIR}/../../../contrib/ntp/libparse

SRCS=	parse.c parse_conf.c \
	clk_computime.c	clk_dcf7000.c	clk_hopf6021.c \
	clk_meinberg.c	clk_rawdcf.c	clk_rcc8000.c \
	clk_schmid.c	clk_trimtaip.c	clk_trimtsip.c \
	clk_varitext.c	clk_wharton.c	data_mbg.c \
	info_trimble.c	trim_info.c

CFLAGS+= -I${.CURDIR}/../../../contrib/ntp/include -I${.CURDIR}/../

NOMAN=                  true
NOPROFILE=              true
INTERNALLIB=            true
NOPIC=                  true
INTERNALSTATICLIB=      true

LIB= parse

.include <bsd.lib.mk>
