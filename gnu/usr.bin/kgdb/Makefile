#	@(#)Makefile	6.4 (Berkley) 5/6/91

PROG=		kgdb
GDBSRCS=	blockframe.c breakpoint.c command.c copying.c core.c \
		cplus-dem.c dbxread.c environ.c eval.c expprint.c \
		expread.y findvar.c infcmd.c inflow.c infrun.c \
		main.c obstack.c printcmd.c regex.c remote.c \
		remote-sl.c source.c stack.c symmisc.c symtab.c \
		utils.c valarith.c valops.c valprint.c values.c \
		version.c
SRCS=           $(CONFIGSRCS) $(GDBSRCS) init.c
CFLAGS+=        -I. -I$(.CURDIR) -I$(.CURDIR)/config \
		-DHAVE_VPRINTF -DKERNELDEBUG -DNEWVM
DPADD+=		$(LIBREADLINE) $(LIBTERM)
LDADD+=		-lreadline -ltermcap
YFLAGS=
.PATH: $(.CURDIR)/config

depend:

.include "config/Makefile.$(MACHINE)"
.include <bsd.prog.mk>

$(OBJS):	param.h

#
# Generate the constructor
#
init.c:		$(CONFIGSRCS) $(GDBSRCS) $(READLINESRCS)
	-((cd $(.CURDIR)/config; \
		egrep -h '^_initialize_[^ ]* *\(\)' $(CONFIGSRCS)); \
	  (cd $(.CURDIR); egrep -h '^_initialize_[^ ]* *\(\)' $(GDBSRCS))) | \
	(echo 'void initialize_all_files () {'; sed -e 's/$$/;/'; echo '}') \
		> init.c

CLEANFILES+=	init.c param.h
