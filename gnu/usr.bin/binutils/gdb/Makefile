# $FreeBSD$

.include "../Makefile.inc0"

GDBDIR= ${.CURDIR}/../../../../contrib/gdb
.PATH:  ${GDBDIR}/gdb ${GDBDIR}/gdb/cli ${GDBDIR}/gdb/mi
.PATH:  ${SRCDIR}/opcodes ${SRCDIR}/binutils

.if ${TARGET_ARCH} == "sparc64"
GDB_CPU=	sparc
.elif ${TARGET_ARCH} == "amd64"
GDB_CPU=	i386
.else
GDB_CPU=	${TARGET_ARCH}
.endif

NOSHARED?=yes
PROG=	gdb
XSRCS=	annotate.c arch-utils.c ax-general.c ax-gdb.c bcache.c		\
	blockframe.c breakpoint.c buildsym.c complaints.c completer.c	\
	copying.c corefile.c corelow.c dcache.c doublest.c event-loop.c	\
	event-top.c environ.c eval.c exec.c expprint.c fork-child.c	\
	frame.c gdb-events.c gdbarch.c gdbtypes.c infcmd.c inflow.c	\
	infptrace.c inf-loop.c infrun.c inftarg.c language.c linespec.c	\
	main.c maint.c mdebugread.c mem-break.c memattr.c minsyms.c	\
	objfiles.c parse.c printcmd.c regcache.c remote.c		\
	remote-utils.c signals.c source.c stabsread.c stack.c symfile.c	\
	symmisc.c symtab.c target.c thread.c top.c tracepoint.c		\
	typeprint.c ui-file.c ui-out.c utils.c valarith.c valops.c	\
	varobj.c valprint.c values.c wrapper.c				\
	cli-cmds.c cli-cmds.h cli-decode.c cli-decode.h	cli-out.c	\
	cli-script.c cli-script.h cli-setshow.c cli-setshow.h		\
	cli-utils.c cli-utils.h						\
	mi-cmd-break.c mi-cmd-disas.c mi-cmd-stack.c mi-cmd-var.c	\
	mi-cmds.c mi-console.c mi-getopt.c mi-main.c mi-out.c mi-parse.c\
	serial.c ser-unix.c ser-tcp.c					\
	c-exp.y c-lang.c c-typeprint.c c-valprint.c			\
	ch-exp.c ch-lang.c ch-typeprint.c ch-valprint.c			\
	cp-abi.c gnu-v2-abi.c gnu-v3-abi.c cp-valprint.c demangle.c	\
	f-exp.y f-lang.c f-typeprint.c f-valprint.c findvar.c		\
	jv-exp.y jv-lang.c jv-valprint.c jv-typeprint.c nlmread.c	\
	m2-lang.c m2-exp.y m2-typeprint.c m2-valprint.c			\
	scm-exp.c scm-lang.c scm-valprint.c				\
	coffread.c dbxread.c dwarfread.c dwarf2read.c elfread.c		\
	solib.c solib-svr4.c solib-legacy.c
XSRCS+=	freebsd-uthread.c kvm-fbsd.c solib-fbsd-kld.c
SRCS=	init.c ${XSRCS} nm.h tm.h xm.h gdbversion.c xregex.h

.include "${.CURDIR}/Makefile.${TARGET_ARCH}"

WARNS=	0
.if ${TARGET_ARCH} != ${MACHINE_ARCH}
CFLAGS+=	-DCROSS_COMPILE=1
.endif
CFLAGS+=	-DDEFAULT_BFD_ARCH=bfd_${GDB_CPU}_arch
CFLAGS+=	-I${.CURDIR}/${TARGET_ARCH}
CFLAGS+=	-I${SRCDIR}/binutils -I${SRCDIR}/bfd
CFLAGS+=	-I${GDBDIR}/gdb -I${GDBDIR}/gdb/config
CFLAGS+=	-I$(.CURDIR) -I${DESTDIR}/usr/include/readline

YFLAGS=

DPADD=		${LIBKVM} ${LIBM}
DPADD+=		${RELTOP}/libbfd/libbfd.a ${RELTOP}/libopcodes/libopcodes.a
DPADD+=		${LIBGNUREGEX}
DPADD+=		${RELTOP}/libiberty/libiberty.a
DPADD+=		${LIBREADLINE} ${LIBTERMCAP}
LDADD=		-lkvm -lm
LDADD+=		${RELTOP}/libbfd/libbfd.a ${RELTOP}/libopcodes/libopcodes.a
LDADD+=		-lgnuregex
LDADD+=		${RELTOP}/libiberty/libiberty.a
LDADD+=		-lreadline -ltermcap

CLEANFILES=	init.c init.c-tmp nm.h tm.h xm.h gdbversion.c xregex.h

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
	@echo '#include "defs.h"'		>>init.c-tmp
	@echo '#include "call-cmds.h"'		>>init.c-tmp
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

nm.h:
	echo '#include "${GDB_CPU}/nm-fbsd.h"'	>${.TARGET}
	echo '#include "fbsd-kgdb.h"'		>>${.TARGET}

.for H in tm-fbsd xm-${GDB_CPU}
${H:C/-.*$//}.h:
	ln -sf \
	    ${.CURDIR}/../../../../contrib/gdb/gdb/config/${GDB_CPU}/${H}.h \
	    ${.TARGET}
.endfor

GDB_VERSION=	"5.2.1 (FreeBSD)"
gdbversion.c:
	@echo '#include "version.h"'				> ${.TARGET}
	@echo 'const char version[] = ${GDB_VERSION};'		>>${.TARGET}
	@echo 'const char host_name[] = "${MACHINE_ARCH}-undermydesk-freebsd";'			>>${.TARGET}
	@echo 'const char target_name[] = "${TARGET_ARCH}-undermydesk-freebsd";'		>>${.TARGET}

xregex.h:
	@echo '#include <gnuregex.h>' >${.TARGET}

.include <bsd.prog.mk>
