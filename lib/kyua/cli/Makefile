# $FreeBSD$

KYUA_LIB=	cli
INTERNALLIB=
LIBADD=		kyua_drivers kyua_engine kyua_model kyua_store kyua_utils
CFLAGS+=	-DKYUA_CONFDIR=\"${KYUA_CONFDIR}\"		\
		-DKYUA_DOCDIR=\"${KYUA_DOCDIR}\"		\
		-DKYUA_MISCDIR=\"${KYUA_MISCDIR}\"		\
		-DPACKAGE=\"kyua\"				\
		-DPACKAGE_NAME=\"Kyua\"				\
		-DPACKAGE_VERSION=\"${KYUA_VERSION}\"		\
		-DVERSION=\"${KYUA_VERSION}\"

SRCS=	cmd_about.cpp		\
	cmd_config.cpp		\
	cmd_db_exec.cpp		\
	cmd_db_migrate.cpp	\
	cmd_debug.cpp		\
	cmd_help.cpp		\
	cmd_list.cpp		\
	cmd_report.cpp		\
	cmd_report_html.cpp	\
	cmd_report_junit.cpp	\
	cmd_test.cpp		\
	common.cpp		\
	config.cpp		\
	main.cpp

.include <bsd.lib.mk>
