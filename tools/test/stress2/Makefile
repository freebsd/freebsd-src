SUBDIR=	lib testcases
MK_AUTO_OBJ=	no

.include <bsd.subdir.mk>

all:	setup

setup:
	@cd ${.CURDIR}; ./tools/setup.sh
test:
	cd ${.CURDIR}/misc; pwd; ./all.sh -onc
