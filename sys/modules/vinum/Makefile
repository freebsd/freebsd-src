# $FreeBSD$

.PATH: ${.CURDIR}/../../dev/vinum

KMOD=	vinum
SRCS=	opt_vinum.h request.h vinum.c vinumconfig.c vinumdaemon.c \
	vinumext.h vinumhdr.h vinuminterrupt.c vinumio.c vinumio.h \
	vinumioctl.c vinumkw.h vinumlock.c vinummemory.c vinumobj.h vinumparser.c \
	vinumraid5.c vinumrequest.c vinumrevive.c vinumstate.c \
	vinumstate.h vinumutil.c vinumvar.h vnode_if.h

# If you remove -DVINUMDEBUG here, you must also do it in src/sbin/Makefile.
# Otherwise a number of ioctls will fail.
CFLAGS+=	-DVINUMDEBUG -g -O

.include <bsd.kmod.mk>
