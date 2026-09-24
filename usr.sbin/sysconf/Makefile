.include <src.opts.mk>

PACKAGE=	utilities
PROG=		sysconf
MAN=		sysconf.8			\
		sysconf-generic.8		\
		sysconf-loader.8		\
		sysconf-make.8			\
		sysconf-rc.8			\
		sysconf-src.8			\
		sysconf-sysctl.8		\
		sysconf-targets.8

SRCS=		sysconf.c			\
		sysconf_edit_assign.c		\
		sysconf_edit_make.c		\
		sysconf_edit_write.c		\
		sysconf_print.c			\
		sysconf_query_desc.c		\
		sysconf_query_sysctl.c		\
		sysconf_query_value.c		\
		sysconf_resolve.c		\
		sysconf_scan.c

CFLAGS+=	-I${SRCTOP}/lib/libbsdconf -I${.CURDIR}

LIBADD=		bsdconf jail

HAS_TESTS=
SUBDIR.${MK_TESTS}+=	tests

.include <bsd.prog.mk>
