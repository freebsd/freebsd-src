#	@(#)Makefile	8.1 (Berkeley) 6/6/93

.if !make(install) && !make(distribute)
# Build tools first so that things don't get built using stale tools and
# then built again after the tools are freshened.
#
# XXX this doesn't fix the problem if the tools are built by running make
# in the tn3270 subdir, because the Makefile doesn't give the full
# dependencies of the tools.
#
# XXX this doesn't fix the problem for `make depend' either.
SUBDIR=	tools
.endif

SUBDIR+=tn3270 mset

.include <bsd.subdir.mk>
