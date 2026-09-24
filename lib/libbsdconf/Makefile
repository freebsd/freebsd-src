PACKAGE=	utilities
LIB=		bsdconf
SHLIB_MAJOR=	1
INCS=		bsdconf.h
MAN=		bsdconf.3 bsdconf_format.3 bsdconf_put.3
MLINKS=		bsdconf.3 bsdconf_fparse.3			\
		bsdconf.3 bsdconf_get_option.3			\
		bsdconf.3 bsdconf_parse.3			\
		bsdconf.3 bsdconf_spool.3			\
		bsdconf.3 bsdconf_unquote.3			\
		bsdconf_format.3 bsdconf_format_derive.3	\
		bsdconf_format.3 bsdconf_format_files.3		\
		bsdconf_format.3 bsdconf_format_files_free.3	\
		bsdconf_format.3 bsdconf_format_find.3		\
		bsdconf_format.3 bsdconf_format_guess.3		\
		bsdconf_format.3 bsdconf_format_lookup.3	\
		bsdconf_format.3 bsdconf_format_path.3		\
		bsdconf_format.3 bsdconf_format_processing.3	\
		bsdconf_format.3 bsdconf_format_put.3		\
		bsdconf_format.3 bsdconf_format_register.3	\
		bsdconf_put.3 bsdconf_set_option.3

CFLAGS+=	-I${.CURDIR}

SRCS=		bsdconf.c bsdconf_format.c bsdconf_format_generic.c	\
		bsdconf_format_loader.c bsdconf_format_make.c		\
		bsdconf_format_src.c bsdconf_format_sysctl.c		\
		bsdconf_put.c bsdconf_stmt.c bsdconf_string.c

.include <bsd.lib.mk>
