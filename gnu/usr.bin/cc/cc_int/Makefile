# $FreeBSD$

.include "../Makefile.inc"

.PATH: ../cc_tools ${GCCDIR}/config/${GCC_ARCH} ${GCCDIR}

LIB=		cc_int
NOPROFILE=	YES
NOPIC=		YES

#	dbxout.c dwarfout.c emit-rtl.c explow.c expmed.c expr.c \
#	alias.c bitmap.c dwarf2out.c dyn-string.c \
SRCS=	c-aux-info.c c-common.c c-convert.c c-format.c \
	c-pragma.c c-semantics.c \
	c-dump.c c-errors.c \
	caller-save.c calls.c combine.c convert.c cse.c \
	dbxout.c dwarfout.c dwarf2out.c dwarf2asm.c emit-rtl.c explow.c expmed.c expr.c \
	final.c flow.c fold-const.c function.c getpwd.c global.c graph.c \
	hash.c haifa-sched.c \
	integrate.c jump.c lcm.c local-alloc.c loop.c optabs.c \
	print-rtl.c print-tree.c real.c recog.c reg-stack.c regclass.c \
	reload.c reload1.c reorg.c rtl.c rtlanal.c \
	sbitmap.c sdbout.c stmt.c stor-layout.c \
	toplev.c tree.c unroll.c varasm.c version.c xcoffout.c \
	alias.c bitmap.c dyn-string.c \
	gcse.c genrtl.c profile.c regmove.c varray.c \
	${OUT_FILE}

SRCS+=	bb-reorder.c conflict.c ggc-common.c \
	ggc-page.c ifcvt.c lists.c predict.c regrename.c resource.c sibcall.c \
	simplify-rtx.c ssa.c timevar.c

# insn-* gunk -- headers are built in cc_tools, as they are used by the
# "build-tools"
.for F in attrtab emit extract opinit output peep recog
SRCS+=	insn-$F.c
CLEANFILES+= insn-$F.c
insn-$F.c: ${.OBJDIR}/../cc_tools/gen$F ${MD_FILE}
	${.OBJDIR}/../cc_tools/gen$F ${MD_FILE} > insn-$F.c
.endfor

#	shared between the drivers
SRCS+=	multilib.h choose-temp.c obstack.c prefix.c pexecute.c version.c mbchar.c

#	libbackend
SRCS+=	builtins.c dce.c diagnostic.c doloop.c dominance.c except.c gcse.c params.c sched-deps.c sched-vis.c sched-rgn.c sched-ebb.c \
	stringpool.c c-lex.c
#	shared by cc1/cc1obj
SRCS+=	#c-decl.c
SRCS+=	c-typeck.c

#	libcpp
SRCS+=	cppdefault.c cpperror.c cppexp.c cppfiles.c cppinit.c cpphash.c \
	cpplex.c cpplib.c cppmacro.c mkdeps.c

#	stuff that makes libiberty now required (2.95) and shouldn't be...
SRCS+=	obstack.c splay-tree.c hashtab.c partition.c lbasename.c md5.c
CFLAGS+=	-DHAVE_CONFIG_H

CFLAGS+=	-DTARGET_NAME=\"${target}\" -DIN_GCC

install:
	@true

${OUT_OBJ}.o ${OUT_OBJ}.So:	${OUT_FILE}

.include <bsd.lib.mk>
