#	%W% (Berkeley) %G%

.include "../config/Makefile.$(MACHINE)"

PROG=		ngdb
SRCS=		i386bsd-dep.c blockframe.c
GDBOBJS+=	i386-pinsn.o \
		breakpoint.o command.o copying.o core.o \
		cplus-dem.o dbxread.o environ.o eval.o expprint.o \
		expread.o findvar.o infcmd.o inflow.o infrun.o \
		main.o obstack.o printcmd.o regex.o remote.o \
		remote-sl.o source.o stack.o symmisc.o symtab.o \
		utils.o valarith.o valops.o valprint.o values.o \
		version.o \
		funmap.o history.o keymaps.o readline.o \
		init.o
CFLAGS+=	-g -I$(.CURDIR) -I.. -I$(.CURDIR)/.. -I$(.CURDIR)/../config \
		-I/usr/src/sys.newvm \
		-DNEWVM -DHAVE_VPRINTF -DVI_MODE -DKERNELDEBUG
# CC=		/usr/old/bin/cc
# CC=		cc -traditional
LDADD+=		$(GDBOBJS:S/^/..\//g) -ltermcap
NOMAN=		noman

.PATH: $(.CURDIR)/../config $(.CURDIR)/..

.include <bsd.prog.mk>
