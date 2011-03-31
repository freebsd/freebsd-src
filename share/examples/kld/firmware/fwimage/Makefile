#	$FreeBSD$

KMOD=	beastie
FIRMWS=	firmware.img:beastie

firmware.img: firmware.img.in
	cp ${.ALLSRC} ${.TARGET}
	printf '\0\0\0\0\0' >> ${.TARGET}

.include <bsd.kmod.mk>
