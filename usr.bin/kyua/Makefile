# $FreeBSD$

.include <src.opts.mk>

.include "${SRCTOP}/lib/kyua/Makefile.kyua"

.PATH: ${KYUA_SRCDIR}

PACKAGE=	tests
PROG_CXX=	kyua
SRCS=		main.cpp
LIBADD=		kyua_cli kyua_drivers kyua_engine kyua_model kyua_store

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

CFLAGS+=	-I${KYUA_SRCDIR}

# kyua uses auto_ptr
CFLAGS+=	-Wno-deprecated-declarations

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
