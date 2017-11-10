# $FreeBSD$

LOADER_ZFS_SUPPORT=yes

.include <bsd.init.mk>

.PATH:	${BOOTSRC}/i386/loader

LOADER=		zfsloader
NEWVERSWHAT=	"ZFS enabled bootstrap loader" x86
LOADER_ONLY=	yes
MAN=

.include "${BOOTSRC}/i386/loader/Makefile"

