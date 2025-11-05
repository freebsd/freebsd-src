.include <src.opts.mk>

PACKAGE?=	libcasper

.if ${MK_CASPER} != "no"
CFLAGS+=-DWITH_CASPER
.endif

.include "../Makefile.inc"
