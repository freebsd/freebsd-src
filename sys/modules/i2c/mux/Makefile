# $FreeBSD$

SUBDIR = \
	iicmux \
	ltc430x \

.if !empty(OPT_FDT)
SUBDIR+= iic_gpiomux
.endif

.include <bsd.subdir.mk>
