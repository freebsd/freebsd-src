# Only parse this if executing make in this directory, not in other places
# in src that lack a Makefile, such as sys/dev/*.  Otherwise the MAKESYSPATH
# will read this Makefile since it auto includes it into -I.
.if ${.CURDIR} == ${.PARSEDIR}

# Avoid creating a Makefile.depend here as it would get included anywhere
# in the build, similar to the problem above.  It would cause dirdeps.mk
# to be included more times than desired.
UPDATE_DEPENDFILE= no

.include <src.opts.mk>

PACKAGE= bmake
FILES=	\
	auto.obj.mk \
	bsd.README \
	bsd.arch.inc.mk \
	bsd.clang-analyze.mk \
	bsd.compiler.mk \
	bsd.compat.mk \
	bsd.compat.pre.mk \
	bsd.confs.mk \
	bsd.cpu.mk \
	bsd.crunchgen.mk \
	bsd.debug.mk \
	bsd.dep.mk \
	bsd.dirs.mk \
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
	bsd.linker.mk \
	bsd.links.mk \
	bsd.man.mk \
	bsd.mkopt.mk \
	bsd.nls.mk \
	bsd.obj.mk \
	bsd.opts.mk \
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
	bsd.suffixes-extra.mk \
	bsd.suffixes-posix.mk \
	bsd.suffixes.mk \
	bsd.symver.mk \
	bsd.sys.mk \
	bsd.sysdir.mk \
	bsd.test.mk \
	dirdeps.mk \
	dirdeps-options.mk \
	dirdeps-targets.mk \
	gendirdeps.mk \
	host-target.mk \
	host.test.mk \
	install-new.mk \
	jobs.mk \
	meta.autodep.mk \
	meta.stage.mk \
	meta.subdir.mk \
	meta.sys.mk \
	meta2deps.py \
	meta2deps.sh \
	${SRCTOP}/contrib/bmake/mk/newlog.sh \
	${SRCTOP}/contrib/bmake/mk/posix.mk \
	stage-install.sh \
	${SRCTOP}/contrib/bmake/mk/setopts.sh \
	sys.mk \
	sys.dependfile.mk \
	sys.dirdeps.mk \
	version_gen.awk

FILESDIR=	${BINDIR}/mk

# make sure scripts are executable
.for file in ${FILES:M*.sh} ${FILES:M*.py}
FILESMODE_${file} = 555
.endfor

.if ${MK_TESTS} != "no"
FILES+=	atf.test.mk
FILES+=	googletest.test.inc.mk
FILES+=	googletest.test.mk
FILES+=	plain.test.mk
FILES+=	suite.test.mk
FILES+=	tap.test.mk
.endif

.include <bsd.prog.mk>
.endif	# CURDIR == PARSEDIR
