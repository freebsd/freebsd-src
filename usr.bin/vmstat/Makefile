.include <src.opts.mk>

PROG=	vmstat
MAN=	vmstat.8
LIBADD=	devstat kvm memstat xo util

HAS_TESTS=
SUBDIR.${MK_TESTS}+= tests

.include <bsd.prog.mk>
