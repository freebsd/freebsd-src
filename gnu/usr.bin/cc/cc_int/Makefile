# $FreeBSD$

.include "../Makefile.inc"

.PATH: ../cc_tools ${GCCDIR}/config/${GCC_CPU} ${GCCDIR}

LIB=		cc_int
INTERNALLIB=

# Files from libiberty.
SRCS=	choose-temp.c concat.c cp-demangle.c cp-demint.c cplus-dem.c \
	dyn-string.c fibheap.c getpwd.c getruntime.c hashtab.c hex.c \
	lbasename.c make-temp-file.c md5.c obstack.c partition.c pex-unix.c \
	physmem.c splay-tree.c xexit.c xmalloc.c xmemdup.c xstrdup.c xstrerror.c

# Language-independent files.
SRCS+=	alias.c bb-reorder.c bitmap.c builtins.c caller-save.c calls.c \
	cfg.c cfganal.c cfgbuild.c cfgcleanup.c cfglayout.c cfgloop.c \
	cfgloopanal.c cfgloopmanip.c loop-init.c loop-unswitch.c loop-unroll.c \
	cfgrtl.c combine.c conflict.c convert.c coverage.c cse.c cselib.c \
	dbxout.c debug.c df.c diagnostic.c dojump.c doloop.c dominance.c \
	dwarf2asm.c dwarf2out.c emit-rtl.c except.c explow.c \
	expmed.c expr.c final.c flow.c fold-const.c function.c gcse.c \
	genrtl.c ggc-common.c global.c graph.c gtype-desc.c \
	haifa-sched.c hooks.c ifcvt.c  \
	integrate.c intl.c jump.c  langhooks.c lcm.c lists.c local-alloc.c \
	loop.c optabs.c options.c opts.c params.c postreload.c predict.c \
	print-rtl.c print-tree.c value-prof.c \
	profile.c ra.c ra-build.c ra-colorize.c ra-debug.c ra-rewrite.c \
	real.c recog.c reg-stack.c regclass.c regmove.c regrename.c \
	reload.c reload1.c reorg.c resource.c rtl.c rtlanal.c rtl-error.c \
	sbitmap.c sched-deps.c sched-ebb.c sched-rgn.c sched-vis.c sdbout.c \
	sibcall.c simplify-rtx.c sreal.c stmt.c stor-layout.c stringpool.c \
	targhooks.c timevar.c toplev.c tracer.c tree.c tree-dump.c unroll.c \
	varasm.c varray.c version.c vmsdbgout.c xcoffout.c alloc-pool.c \
	et-forest.c cfghooks.c bt-load.c pretty-print.c ggc-page.c web.c

# Miscellaneous files.
SRCS+=	hashtable.c tree-inline.c tree-optimize.c cgraph.c cgraphunit.c

# Host hooks.
SRCS+=	host-default.c

# C and ObjC files.
SRCS+=	attribs.c c-errors.c c-lex.c c-pragma.c c-decl.c c-typeck.c \
	c-convert.c c-aux-info.c c-common.c c-opts.c c-format.c c-semantics.c \
	c-incpath.c cppdefault.c c-ppoutput.c c-cppbuiltin.c prefix.c \
	c-objc-common.c c-dump.c c-pch.c

# Files shared by cc1plus, cc1 and cc1obj
SRCS+=	c-pretty-print.c

# Machine-dependent files.
SRCS+=	${GCC_CPU}.c
.if exists(${GCCDIR}/config/${GCC_CPU}/${GCC_CPU}-c.c)
SRCS+=	${GCC_CPU}-c.c
.endif

# libcpp files.
SRCS+=	cpplib.c cpplex.c cppmacro.c cppexp.c cppfiles.c cpptrad.c \
	cpphash.c cpperror.c cppinit.c cppcharset.c \
	line-map.c mkdeps.c cpppch.c

# insn-* gunk -- headers are built in cc_tools, as they are used by the
# "build-tools"
.for F in attrtab emit extract opinit output peep recog
SRCS+=	insn-$F.c
CLEANFILES+= insn-$F.c
insn-$F.c: ${.OBJDIR}/../cc_tools/gen$F ${MD_FILE}
	${.OBJDIR}/../cc_tools/gen$F ${MD_FILE} > insn-$F.c
.endfor
.for F in modes
SRCS+=	insn-$F.c
CLEANFILES+= insn-$F.c
insn-$F.c: ${.OBJDIR}/../cc_tools/gen$F
	${.OBJDIR}/../cc_tools/gen$F > insn-$F.c
.endfor

CFLAGS+=	-DHAVE_CONFIG_H
CFLAGS+=	-DTARGET_NAME=\"${target}\" -DIN_GCC

# c-pch.o needs extra defines. Replicate the rule here rather than
# pollute compiler command line for all other files.
c-pch.o: c-pch.c
	${CC} ${CFLAGS} -DHOST_MACHINE=\"${host}\" \
		 -DTARGET_MACHINE=\"${target}\" -c ${.IMPSRC}

.include <bsd.lib.mk>
