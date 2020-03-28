# $FreeBSD$

.include "${SRCTOP}/lib/kyua/Makefile.kyua"

.PATH: ${.CURDIR} ${.OBJDIR} ${KYUA_SRCDIR}

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

FILESGROUPS=	DOCS EXAMPLES MISC STORE

CONFS=		kyua.conf
CONFSDIR=	${KYUA_CONFDIR}

DOCS=		AUTHORS CONTRIBUTORS LICENSE
DOCSDIR=	${KYUA_DOCDIR}

EXAMPLES=	Kyuafile.top kyua.conf
EXAMPLESDIR=	${KYUA_EGDIR}
.PATH:		${KYUA_SRCDIR}/examples

MISC=		context.html index.html report.css test_result.html
MISCDIR=	${KYUA_MISCDIR}
.PATH:		${KYUA_SRCDIR}/misc

STORE=		migrate_v1_v2.sql migrate_v2_v3.sql schema_v3.sql
STOREDIR=	${KYUA_STOREDIR}
.PATH:		${KYUA_SRCDIR}/store

.PATH: ${KYUA_SRCDIR}/doc

.include <bsd.prog.mk>
