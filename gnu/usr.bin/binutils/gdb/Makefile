# $FreeBSD$

.include "../Makefile.inc0"

.PATH: ${SRCDIR}/binutils

.if ${MACHINE_ARCH} == "i386"
CFLAGS+=	-Dprint_insn_i386=print_insn_i386_att
.endif

PROG=	gdb
XSRCS=	annotate.c ax-general.c ax-gdb.c bcache.c blockframe.c		\
	breakpoint.c buildsym.c c-exp.y c-lang.c c-typeprint.c		\
	c-valprint.c ch-exp.c ch-lang.c ch-typeprint.c ch-valprint.c	\
	coffread.c command.c complaints.c copying.c core-regset.c	\
	corefile.c corelow.c cp-valprint.c dcache.c dbxread.c		\
	demangle.c dwarfread.c dwarf2read.c elfread.c environ.c eval.c	\
	exec.c expprint.c f-exp.y f-lang.c f-typeprint.c f-valprint.c	\
	findvar.c fork-child.c freebsd-uthread.c gdbarch.c gdbtypes.c	\
	infcmd.c inflow.c infptrace.c infrun.c inftarg.c language.c	\
	jv-exp.y jv-lang.c jv-valprint.c jv-typeprint.c nlmread.c	\
	m2-lang.c m2-exp.y m2-typeprint.c m2-valprint.c main.c maint.c	\
	mdebugread.c mem-break.c minsyms.c objfiles.c parse.c		\
	printcmd.c remote.c remote-utils.c scm-exp.c scm-lang.c		\
	scm-valprint.c solib.c source.c stabsread.c stack.c symfile.c	\
	symmisc.c symtab.c target.c thread.c top.c tracepoint.c		\
	typeprint.c utils.c valarith.c valops.c valprint.c values.c	\
	version.c serial.c ser-unix.c ser-tcp.c
SRCS=	init.c ${XSRCS}
SRCS+=	wait.h

.if exists(${.CURDIR}/Makefile.${MACHINE_ARCH})
.include "${.CURDIR}/Makefile.${MACHINE_ARCH}"
.endif

WARNS=	0
#CFLAGS+=	-I${.CURDIR}/${RELTOP}/libbinutils
CFLAGS+=	-I${.CURDIR}/${MACHINE_ARCH}
CFLAGS+=	-I${SRCDIR}/binutils
CFLAGS+=	-I${SRCDIR}/bfd
CFLAGS+=	-I${GDBDIR}/gdb
CFLAGS+=	-I${GDBDIR}/gdb/config
DPADD=		${RELTOP}/libbfd/libbfd.a
DPADD+=		${RELTOP}/libopcodes/libopcodes.a
DPADD+=		${LIBREADLINE}
DPADD+=		${LIBGNUREGEX}
DPADD+=		${RELTOP}/libiberty/libiberty.a
DPADD+=		${LIBTERMCAP}
LDADD+=		${RELTOP}/libbfd/libbfd.a
LDADD+=		${RELTOP}/libopcodes/libopcodes.a
LDADD+=		-lreadline
LDADD+=		-lgnuregex
LDADD+=		${RELTOP}/libiberty/libiberty.a
LDADD+=		-ltermcap

GDBDIR=	${.CURDIR}/../../../../contrib/gdb
.PATH:	${GDBDIR}/gdb
.PATH:	${SRCDIR}/opcodes

CFLAGS+=	-DFREEBSD_ELF

CFLAGS+= -I$(.CURDIR) -I${DESTDIR}/usr/include/readline
# use phkmalloc
CFLAGS+= -DNO_MMALLOC
# uncomment the next line if you want to debug gdb
#CFLAGS+= -g
YFLAGS=

CLEANFILES=	init.c init.c-tmp wait.h

# We do this by grepping through sources.  If that turns out to be too slow,
# maybe we could just require every .o file to have an initialization routine
# of a given name (remote-udi.o -> _initialize_remote_udi, etc.).
#
# Formatting conventions:  The name of the _initialize_* routines must start
# in column zero, and must not be inside #if.
#
# Note that the set of files with init functions might change, or the names
# of the functions might change, so this files needs to depend on all the
# object files that will be linked into gdb.

init.c: ${XSRCS}
	@${ECHO} Making ${.TARGET}
	@rm -f init.c-tmp
	@echo '/* Do not modify this file.  */' >init.c-tmp
	@echo '/* It is created automatically by the Makefile.  */'>>init.c-tmp
	@echo 'void initialize_all_files () {' >>init.c-tmp
	@for i in ${.ALLSRC} ; do \
	  filename=`echo $$i | sed \
	    -e '/^Onindy.c/d' \
	    -e '/^nindy.c/d' \
	    -e '/ttyflush.c/d' \
	    -e '/xdr_ld.c/d' \
	    -e '/xdr_ptrace.c/d' \
	    -e '/xdr_rdb.c/d' \
	    -e '/udr.c/d' \
	    -e '/udip2soc.c/d' \
	    -e '/udi2go32.c/d' \
	    -e '/version.c/d' \
	    -e '/^[a-z0-9A-Z_]*_[SU].c/d' \
	    -e '/[a-z0-9A-Z_]*-exp.tab.c/d'` ; \
	  case $$filename in \
	    "") ;; \
	    *) sed <$$filename >>init.c-tmp -n \
	-e '/^_initialize_[a-z_0-9A-Z]* *(/s/^\([a-z_0-9A-Z]*\).*/  {extern void \1 (); \1 ();}/p' ; ;; \
	  esac ; \
	done
	@echo '}' >>init.c-tmp
	@mv init.c-tmp ${.TARGET}

.PRECIOUS: init.c

wait.h:
	ln -sf ${.CURDIR}/../../../../sys/sys/wait.h ${.TARGET}

.include <bsd.prog.mk>
