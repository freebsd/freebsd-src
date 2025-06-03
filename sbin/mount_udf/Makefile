PACKAGE=runtime
PROG=	mount_udf
MAN=	mount_udf.8
LIBADD=	kiconv util

# Needs to be dynamically linked for optional dlopen() access to
# userland libiconv
NO_SHARED?=	NO

.include <bsd.prog.mk>
