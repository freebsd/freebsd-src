# $FreeBSD$

LIB=	gdb
INTERNALLIB=
SRCS=	annotate.c arch-utils.c auxv.c ax-gdb.c ax-general.c \
	bcache.c bfd-target.c block.c blockframe.c breakpoint.c \
	buildsym.c \
	c-exp.y c-lang.c c-typeprint.c c-valprint.c charset.c \
	cli-cmds.c cli-decode.c cli-dump.c cli-interp.c cli-logging.c \
	cli-out.c cli-script.c cli-setshow.c cli-utils.c coff-pe-read.c \
	coffread.c complaints.c completer.c copying.c corefile.c \
	corelow.c cp-abi.c cp-namespace.c cp-support.c cp-valprint.c \
	dbxread.c dcache.c demangle.c dictionary.c disasm.c doublest.c \
	dummy-frame.c dwarf2-frame.c dwarf2expr.c dwarf2loc.c \
	dwarf2read.c dwarfread.c \
	elfread.c environ.c eval.c event-loop.c event-top.c exec.c \
	expprint.c \
	f-exp.y f-lang.c f-typeprint.c f-valprint.c findvar.c \
	fork-child.c frame-base.c frame-unwind.c frame.c \
	gdb-events.c gdbarch.c gdbtypes.c gnu-v2-abi.c gnu-v3-abi.c \
	hpacc-abi.c \
	inf-loop.c infcall.c infcmd.c inflow.c infptrace.c infrun.c \
	inftarg.c init.c interps.c \
	jv-exp.y jv-lang.c jv-typeprint.c jv-valprint.c	\
	kod-cisco.c kod.c \
	language.c linespec.c \
	m2-exp.y m2-lang.c m2-typeprint.c m2-valprint.c macrocmd.c \
	macroexp.c macroscope.c	macrotab.c main.c maint.c mdebugread.c \
	mem-break.c memattr.c mi-cmd-break.c mi-cmd-disas.c \
	mi-cmd-env.c mi-cmd-file.c mi-cmd-stack.c mi-cmd-var.c \
	mi-cmds.c mi-console.c mi-getopt.c mi-interp.c mi-main.c \
	mi-out.c mi-parse.c mi-symbol-cmds.c minsyms.c mipsread.c \
	nlmread.c \
	objc-exp.y objc-lang.c objfiles.c observer.c osabi.c \
	p-exp.y p-lang.c p-typeprint.c p-valprint.c parse.c printcmd.c \
	regcache.c reggroups.c remote-fileio.c remote-utils.c remote.c \
	scm-exp.c scm-lang.c scm-valprint.c sentinel-frame.c ser-pipe.c \
	ser-tcp.c ser-unix.c serial.c signals.c solib.c solib-svr4.c \
	source.c stabsread.c stack.c std-regs.c symfile.c symmisc.c \
	symtab.c \
	target.c thread.c top.c tracepoint.c trad-frame.c tui-command.c \
	tui-data.c tui-disasm.c tui-file.c tui-hooks.c tui-interp.c \
	tui-io.c tui-layout.c tui-out.c tui-regs.c tui-source.c \
	tui-stack.c tui-win.c tui-windata.c tui-wingeneral.c \
	tui-winsource.c tui.c typeprint.c \
	ui-file.c ui-out.c user-regs.c utils.c \
	valarith.c valops.c valprint.c values.c varobj.c \
	wrapper.c ${LIBSRCS}

GENSRCS= version.c

.if ${MACHINE_ARCH} == "amd64" || ${MACHINE_ARCH} == "i386"
LIBSRCS+= fbsd-threads.c
.endif

.for stupid_gnu in \
xregcomp xre_exec xregexec xre_search xre_compile_fastmap xregerror xre_comp xre_set_syntax
CFLAGS+= -D${stupid_gnu}=${stupid_gnu:S/^x//}
.endfor

version.c:
	echo '#include "version.h"' >  ${.TARGET}
	echo 'const char version[] = ${VERSION};' >> ${.TARGET}
	echo 'const char host_name[] = "${MACHINE_ARCH}-${VENDOR}-freebsd";' \
	    >> ${.TARGET}
	echo 'const char target_name[] = "${TARGET_ARCH}-${VENDOR}-freebsd";' \
	    >> ${.TARGET}

.include <bsd.lib.mk>
