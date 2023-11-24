
.include <src.opts.mk>

KYUA_CONFDIR=	/etc/kyua
KYUA_DOCDIR=	/usr/share/doc/kyua
KYUA_EGDIR=	/usr/share/examples/kyua
KYUA_MISCDIR=	/usr/share/kyua/misc
KYUA_STOREDIR=	/usr/share/kyua/store
KYUA_VERSION=	0.13

KYUA_SRCDIR=	${SRCTOP}/contrib/kyua
.PATH: ${KYUA_SRCDIR}

PACKAGE=	tests
PROG_CXX=	kyua
SRCS=		main.cpp
LIBADD=		lutok sqlite3

MAN=		kyua-about.1 \
		kyua-config.1 \
		kyua-db-exec.1 \
		kyua-db-migrate.1 \
		kyua-debug.1 \
		kyua-help.1 \
		kyua-list.1 \
		kyua-report-html.1 \
		kyua-report-junit.1 \
		kyua-report.1 \
		kyua-test.1 \
		kyua.1 \
		kyua.conf.5 \
		kyuafile.5

CFLAGS+=	-I${KYUA_SRCDIR} -I${.CURDIR}
CFLAGS+=	-I${SRCTOP}/contrib/lutok/include
CFLAGS+=	-I${SRCTOP}/contrib/sqlite3
# kyua uses auto_ptr
CFLAGS+=	-Wno-deprecated-declarations
CXXSTD=		c++11
CFLAGS+=	-DHAVE_CONFIG_H

# We compile the kyua libraries as part of the main executable as this saves
# compile time and we don't install them anyway.
CFLAGS+=	-DGDB=\"/usr/local/bin/gdb\"			\
		-DKYUA_ARCHITECTURE=\"${MACHINE_ARCH}\"		\
		-DKYUA_CONFDIR=\"${KYUA_CONFDIR}\"		\
		-DKYUA_DOCDIR=\"${KYUA_DOCDIR}\"		\
		-DKYUA_MISCDIR=\"${KYUA_MISCDIR}\"		\
		-DKYUA_PLATFORM=\"${MACHINE}\"			\
		-DKYUA_STOREDIR=\"${KYUA_STOREDIR}\"		\
		-DPACKAGE=\"kyua\"				\
		-DPACKAGE_NAME=\"Kyua\"				\
		-DPACKAGE_VERSION=\"${KYUA_VERSION}\"		\
		-DVERSION=\"${KYUA_VERSION}\"

SRCS+=	utils/datetime.cpp			\
	utils/env.cpp				\
	utils/memory.cpp			\
	utils/passwd.cpp			\
	utils/sanity.cpp			\
	utils/stacktrace.cpp			\
	utils/stream.cpp			\
	utils/units.cpp				\
	utils/cmdline/base_command.cpp		\
	utils/cmdline/exceptions.cpp		\
	utils/cmdline/globals.cpp		\
	utils/cmdline/options.cpp		\
	utils/cmdline/parser.cpp		\
	utils/cmdline/ui.cpp			\
	utils/cmdline/ui_mock.cpp		\
	utils/config/exceptions.cpp		\
	utils/config/keys.cpp			\
	utils/config/lua_module.cpp		\
	utils/config/nodes.cpp			\
	utils/config/parser.cpp			\
	utils/config/tree.cpp			\
	utils/format/exceptions.cpp		\
	utils/format/formatter.cpp		\
	utils/fs/auto_cleaners.cpp		\
	utils/fs/directory.cpp			\
	utils/fs/exceptions.cpp			\
	utils/fs/lua_module.cpp			\
	utils/fs/operations.cpp			\
	utils/fs/path.cpp			\
	utils/logging/operations.cpp		\
	utils/process/child.cpp			\
	utils/process/deadline_killer.cpp	\
	utils/process/exceptions.cpp		\
	utils/process/executor.cpp		\
	utils/process/fdstream.cpp		\
	utils/process/isolation.cpp		\
	utils/process/operations.cpp		\
	utils/process/status.cpp		\
	utils/process/system.cpp		\
	utils/process/systembuf.cpp		\
	utils/signals/exceptions.cpp		\
	utils/signals/interrupts.cpp		\
	utils/signals/misc.cpp			\
	utils/signals/programmer.cpp		\
	utils/signals/timer.cpp			\
	utils/sqlite/c_gate.cpp			\
	utils/sqlite/database.cpp		\
	utils/sqlite/exceptions.cpp		\
	utils/sqlite/statement.cpp		\
	utils/sqlite/transaction.cpp		\
	utils/text/exceptions.cpp		\
	utils/text/operations.cpp		\
	utils/text/regex.cpp			\
	utils/text/table.cpp			\
	utils/text/templates.cpp

SRCS+=	model/context.cpp	\
	model/exceptions.cpp	\
	model/metadata.cpp	\
	model/test_case.cpp	\
	model/test_program.cpp	\
	model/test_result.cpp

SRCS+=	engine/atf.cpp			\
	engine/atf_list.cpp		\
	engine/atf_result.cpp		\
	engine/config.cpp		\
	engine/exceptions.cpp		\
	engine/filters.cpp		\
	engine/kyuafile.cpp		\
	engine/plain.cpp		\
	engine/requirements.cpp		\
	engine/scanner.cpp		\
	engine/tap.cpp			\
	engine/tap_parser.cpp		\
	engine/scheduler.cpp

SRCS+=	store/dbtypes.cpp		\
	store/exceptions.cpp		\
	store/layout.cpp		\
	store/metadata.cpp		\
	store/migrate.cpp		\
	store/read_backend.cpp		\
	store/read_transaction.cpp	\
	store/write_backend.cpp		\
	store/write_transaction.cpp

SRCS+=	drivers/debug_test.cpp		\
	drivers/list_tests.cpp		\
	drivers/report_junit.cpp	\
	drivers/run_tests.cpp		\
	drivers/scan_results.cpp

SRCS+=	cli/cmd_about.cpp		\
	cli/cmd_config.cpp		\
	cli/cmd_db_exec.cpp		\
	cli/cmd_db_migrate.cpp		\
	cli/cmd_debug.cpp		\
	cli/cmd_help.cpp		\
	cli/cmd_list.cpp		\
	cli/cmd_report.cpp		\
	cli/cmd_report_html.cpp		\
	cli/cmd_report_junit.cpp	\
	cli/cmd_test.cpp		\
	cli/common.cpp			\
	cli/config.cpp			\
	cli/main.cpp

FILESGROUPS=	DOCS MISC STORE

.if ${MK_EXAMPLES} != "no"
FILESGROUPS+=	EXAMPLES
.endif

# Install a minimal default config that uses the 'tests' user.
# The examples config is not appropriate for general use.
CONFS=		kyua.conf-default
CONFSDIR=	${KYUA_CONFDIR}
CONFSNAME=	kyua.conf
CONFSDIRTAGS=	package=tests

DOCS=		AUTHORS CONTRIBUTORS LICENSE
DOCSDIR=	${KYUA_DOCDIR}
DOCSTAGS=	package=tests

EXAMPLES=	Kyuafile.top kyua.conf
EXAMPLESDIR=	${KYUA_EGDIR}
EXAMPLESTAGS=	package=tests
.PATH:		${KYUA_SRCDIR}/examples

MISC=		context.html index.html report.css test_result.html
MISCDIR=	${KYUA_MISCDIR}
MISCTAGS=	package=tests
.PATH:		${KYUA_SRCDIR}/misc

STORE=		migrate_v1_v2.sql migrate_v2_v3.sql schema_v3.sql
STOREDIR=	${KYUA_STOREDIR}
STORETAGS=	package=tests
.PATH:		${KYUA_SRCDIR}/store

CLEANFILES+=	${MAN}
.PATH: ${KYUA_SRCDIR}/doc
.for man in ${MAN}
${man}: ${man}.in
	sh ${KYUA_SRCDIR}/doc/manbuild.sh \
	    -v "CONFDIR=${KYUA_CONFDIR}" \
	    -v "DOCDIR=${KYUA_DOCDIR}" \
	    -v "EGDIR=${KYUA_EGDIR}" \
	    -v "MISCDIR=${KYUA_MISCDIR}" \
	    -v "PACKAGE=kyua" \
	    -v "STOREDIR=${KYUA_STOREDIR}" \
	    -v "TESTSDIR=${TESTSBASE}" \
	    -v "VERSION=${KYUA_VERSION}" \
	    ${.ALLSRC} ${.TARGET}
.endfor

.include <bsd.prog.mk>
