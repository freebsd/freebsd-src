# $FreeBSD$
#	@(#)Makefile	8.1 (Berkeley) 6/8/93

FILES=	bsd.README
FILES+=	bsd.cpu.mk bsd.dep.mk bsd.doc.mk bsd.docb.mk bsd.info.mk bsd.kern.mk bsd.kmod.mk
FILES+=	bsd.lib.mk bsd.libnames.mk bsd.man.mk bsd.obj.mk bsd.own.mk
FILES+=	bsd.port.mk bsd.port.post.mk  bsd.port.pre.mk bsd.port.subdir.mk
FILES+=	bsd.prog.mk bsd.sgml.mk bsd.subdir.mk bsd.sys.mk
FILES+=	sys.mk
NOOBJ=	noobj
FILESDIR=	${BINDIR}/mk

beforeinstall:
	date '+%Y%m%d' > ${DESTDIR}/var/db/port.mkversion

.include <bsd.prog.mk>
