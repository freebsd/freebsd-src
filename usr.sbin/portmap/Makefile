#	@(#)Makefile	8.1 (Berkeley) 6/6/93

PROG=	portmap
MAN8=	portmap.8
SRCS=	portmap.c from_local.c pmap_check.c
SUBDIR= pmap_set pmap_dump

# -DHOSTS_ACCESS (requires tcpwrapper libraries)
CFLAGS+=-DCHECK_PORT

.include <bsd.prog.mk>
