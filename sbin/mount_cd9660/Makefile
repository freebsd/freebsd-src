PACKAGE=runtime
PROG=	mount_cd9660
MAN=	mount_cd9660.8
LIBADD=	kiconv util

# Needs to be dynamically linked for optional dlopen() access to
# userland libiconv
NO_SHARED?=	NO

.include <bsd.prog.mk>
