
SUBDIR = \
	iicmux \
	ltc430x \
	pca954x \

.if !empty(OPT_FDT)
SUBDIR+= iic_gpiomux
.endif

.include <bsd.subdir.mk>
