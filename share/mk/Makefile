# $FreeBSD$
#	@(#)Makefile	8.1 (Berkeley) 6/8/93

.include <bsd.own.mk>

FILES=	\
	bsd.README \
	bsd.arch.inc.mk \
	bsd.compiler.mk \
	bsd.cpu.mk \
	bsd.crunchgen.mk \
	bsd.dep.mk \
	bsd.doc.mk \
	bsd.dtrace.mk \
	bsd.endian.mk \
	bsd.files.mk \
	bsd.incs.mk \
	bsd.info.mk \
	bsd.init.mk \
	bsd.kmod.mk \
	bsd.lib.mk \
	bsd.libnames.mk \
	bsd.links.mk \
	bsd.man.mk \
	bsd.nls.mk \
	bsd.obj.mk \
	bsd.own.mk \
	bsd.port.mk \
	bsd.port.options.mk \
	bsd.port.post.mk \
	bsd.port.pre.mk \
	bsd.port.subdir.mk \
	bsd.prog.mk \
	bsd.progs.mk \
	bsd.snmpmod.mk \
	bsd.subdir.mk \
	bsd.symver.mk \
	bsd.sys.mk \
	bsd.test.mk \
	sys.mk \
	version_gen.awk

NO_OBJ=
FILESDIR=	${BINDIR}/mk

.if ${MK_ATF} != "no"
FILES+=	atf.test.mk
.endif

.include <bsd.prog.mk>
