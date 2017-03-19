# $FreeBSD$

.include <src.opts.mk>

# broken: "mdconfig (attach) exited with error code 1" for mdmfs during boot
WANT_CHERI=none

PACKAGE=runtime
PROG=		mdconfig
MAN=		mdconfig.8

LIBADD=		util geom

.if ${MK_TESTS} != "no"
SUBDIR+=    tests
.endif

.include <bsd.prog.mk>
