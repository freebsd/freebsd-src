#	%W% (Berkeley) %G%

.include "../config/Makefile.$(MACHINE)"

PROG=		xgdb
SRCS=		xgdb.c xgdbinit.c
GDBOBJS+=	$(CONFIGSRCS:R:S/$/.o/g) \
		blockframe.o breakpoint.o command.o copying.o core.o \
		cplus-dem.o dbxread.o environ.o eval.o expprint.o \
		expread.o findvar.o infcmd.o inflow.o infrun.o \
		main.o obstack.o printcmd.o regex.o remote.o \
		remote-sl.o source.o stack.o symmisc.o symtab.o \
		utils.o valarith.o valops.o valprint.o values.o \
		version.o \
		funmap.o history.o keymaps.o readline.o
CFLAGS+=	-I.. -I$(.CURDIR)/.. -I$(.CURDIR)/../config \
		-DHAVE_VPRINTF -DVI_MODE -DKERNELDEBUG
# XXX objects should be in OBJS so that bsd.prog.mk handles their dependencies
DPADD+=		$(GDBOBJS:S/^/..\//g)
# XXX cloned X lib stuff from xditview/Makefile.  Should include it
LDDESTDIR=	/usr/X386/lib
DPADD+=		${LDDESTDIR}/libXaw.a ${LDDESTDIR}/libXmu.a \
		${LDDESTDIR}/libXt.a ${LDDESTDIR}/libXext.a \
		${LDDESTDIR}/libX11.a
DPADD+=		${LINTERM}
LDADD+=		$(GDBOBJS:S/^/..\//g) -lXaw -lXmu -lXt -lXext -lX11 -ltermcap
NOMAN=		noman

.include "../../Makefile.inc"
.include <bsd.prog.mk>

#
# Generate the constructor
#
xgdbinit.c:	../init.c xgdb.c
	-(sed -e '/^}$$/d' ../init.c; \
	  egrep -h '^_initialize_[^ ]* *\(\)' $(.CURDIR)/xgdb.c; \
	  echo ';}') > xgdbinit.c

CLEANFILES+=	xgdbinit.c
