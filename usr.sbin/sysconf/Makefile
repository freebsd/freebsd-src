.include <src.opts.mk>

PACKAGE=	utilities
PROG=		sysconf
MAN=		sysconf.8			\
		sysconf-generic.8		\
		sysconf-loader.8		\
		sysconf-make.8			\
		sysconf-pkg.8			\
		sysconf-poudriere.8		\
		sysconf-rc.8			\
		sysconf-src.8			\
		sysconf-sysctl.8		\
		sysconf-targets.8		\
		sysconf-ucl.8

SRCS=		sysconf.c			\
		sysconf-targets.c		\
		sysconf_edit_assign.c		\
		sysconf_edit_make.c		\
		sysconf_edit_write.c		\
		sysconf_pkg.c			\
		sysconf_poudriere.c		\
		sysconf_print.c			\
		sysconf_query_desc.c		\
		sysconf_query_sysctl.c		\
		sysconf_query_value.c		\
		sysconf_resolve.c		\
		sysconf_scan.c			\
		sysconf_ucl.c			\
		sysconf_ucl_array.c		\
		sysconf_ucl_file.c		\
		sysconf_ucl_index.c		\
		sysconf_ucl_lex.c		\
		sysconf_ucl_list.c		\
		sysconf_ucl_style.c		\
		sysconf_ucl_text.c		\
		sysconf_ucl_write.c

CFLAGS+=	-I${SRCTOP}/lib/libbsdconf -I${.CURDIR}
CFLAGS+=	-I${SRCTOP}/contrib/libucl/include

LIBADD=		bsdconf jail ucl

HAS_TESTS=
SUBDIR.${MK_TESTS}+=	tests

.include <bsd.prog.mk>
