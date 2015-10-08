# $FreeBSD$
#	@(#)Makefile	8.1 (Berkeley) 6/8/93

# Only parse this if executing make in this directory, not in other places
# in src that lack a Makefile, such as sys/dev/*.  Otherwise the MAKESYSPATH
# will read this Makefile since it auto includes it into -I.
# Note that this guard only works for bmake.
.if !defined(.PARSEDIR) || ${.CURDIR} == ${.PARSEDIR}

.include <bsd.own.mk>

FILES=	\
	bsd.README \
	bsd.arch.inc.mk \
	bsd.compiler.mk \
	bsd.cpu.mk \
	bsd.crunchgen.mk \
	bsd.dep.mk \
	bsd.doc.mk \
	bsd.dtb.mk \
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

.if ${MK_TESTS} != "no"
FILES+=	atf.test.mk
FILES+=	plain.test.mk
FILES+=	suite.test.mk
FILES+=	tap.test.mk
.endif

.include <bsd.prog.mk>
.endif	# CURDIR == PARSEDIR
