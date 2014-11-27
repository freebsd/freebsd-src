# $FreeBSD$

.include <src.opts.mk>

SUBDIR=	console \
	diskmgmt \
	docsinstall \
	dot \
	include \
	includes \
	mouse \
	networking \
	packages \
	password \
	security \
	share \
	startup \
	timezone \
	ttys \
	usermgmt

FILESDIR=	${LIBEXECDIR}/bsdconfig
FILES=		USAGE

SCRIPTS= bsdconfig

MAN= bsdconfig.8

.if ${MK_EXAMPLES} != "no"
SUBDIR+=	examples
.endif

.include <bsd.prog.mk>
