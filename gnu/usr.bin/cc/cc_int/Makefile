# $FreeBSD$

.include "../Makefile.inc"

.PATH: ../cc_tools ${GCCDIR}/config/${TARGET_ARCH} ${GCCDIR}

LIB=		cc_int
NOPROFILE=	YES
NOPIC=		YES

SRCS=	c-aux-info.c c-common.c c-convert.c c-iterate.c \
	c-pragma.c c-typeck.c \
	caller-save.c calls.c combine.c convert.c cse.c \
	dbxout.c dwarfout.c emit-rtl.c explow.c expmed.c expr.c \
	final.c flow.c fold-const.c function.c getpwd.c global.c graph.c \
	hash.c \
	integrate.c jump.c lcm.c local-alloc.c loop.c optabs.c \
	print-rtl.c print-tree.c real.c recog.c reg-stack.c regclass.c \
	reload.c reload1.c reorg.c rtl.c rtlanal.c \
	sbitmap.c sdbout.c stmt.c stor-layout.c stupid.c \
	toplev.c tree.c unroll.c varasm.c version.c xcoffout.c \
	alias.c bitmap.c dwarf2out.c dyn-string.c except.c \
	gcse.c genrtl.c profile.c regmove.c varray.c \
	${TARGET_ARCH}.c
.if defined(USE_EGCS_HAIFA) && ${USE_EGCS_HAIFA} == 1
SRCS+=	haifa-sched.c
.else
SRCS+=	sched.c
.endif

# insn-* gunk -- headers are built in cc_tools, as they are used by the
# "build-tools"
.for F in attrtab emit extract opinit output peep recog
SRCS+=	insn-$F.c
CLEANFILES+= insn-$F.c
insn-$F.c: ${.OBJDIR}/../cc_tools/gen$F ${MD_FILE}
	${.OBJDIR}/../cc_tools/gen$F ${MD_FILE} > insn-$F.c
.endfor

#	shared between the drivers
SRCS+=	multilib.h choose-temp.c obstack.c prefix.c pexecute.c version.c

#	stuff that makes libiberty now required (2.95) and shouldn't be...
SRCS+=	obstack.c splay-tree.c

CFLAGS+=	-DTARGET_NAME=\"${target}\" -DIN_GCC

install:
	@true

.include <bsd.lib.mk>
