PROG=	communications-performance
SRCS=	communications-performance.c \
	wayland-itc-buffer.c \
	wayland-itc-buffer.h

# Note: wayland-itc-buffer.[ch] have been modified from the Wayland versions
# (which are wayland-itc-buffer_upstream.[ch]). The Wayland versions are
# unchanged: all modifications are kept in the .patch file.
wayland-itc-buffer.c wayland-itc-buffer.h: wayland-itc-buffer.patch \
		wayland-itc-buffer_upstream.c wayland-itc-buffer_upstream.h
	cp -f ${.CURDIR}/wayland-itc-buffer_upstream.c wayland-itc-buffer.c
	cp -f ${.CURDIR}/wayland-itc-buffer_upstream.h wayland-itc-buffer.h
	patch -p0 -i ${.CURDIR}/wayland-itc-buffer.patch

CLEANFILES=	wayland-itc-buffer.c \
		wayland-itc-buffer.h

FILES=	wayland-itc-buffer_upstream.c \
	wayland-itc-buffer_upstream.h

MAN=

# The -I is needed to ensure we use the patched wayland-itc-buffer.h
CFLAGS=	-DCP2 -pthread -I${.OBJDIR}

.include <bsd.prog.mk>
