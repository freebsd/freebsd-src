# $FreeBSD: src/sys/boot/alpha/cdboot/Makefile,v 1.7 2002/05/10 09:26:28 obrien Exp $

BASE= 		cdboot
PROG= 		${BASE}
NOMAN=
NEWVERSWHAT=	"SRM CD9660 boot" alpha
LOAD_ADDRESS=	${PRIMARY_LOAD_ADDRESS}

CFLAGS+=	-ffreestanding -DLOADER_CDROM_SUPPORT

.include <${.CURDIR}/../common/Makefile.common>
