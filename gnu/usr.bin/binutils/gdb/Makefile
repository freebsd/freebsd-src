#	$Id: Makefile,v 1.20 1997/03/29 10:33:13 peter Exp $

PROG = gdb

GDBDIR=	${.CURDIR}/../../../../contrib/gdb
.PATH:	${GDBDIR}/gdb
.PATH:	${GDBDIR}/opcodes

BINDIR= /usr/bin
SRCS = annotate.c blockframe.c breakpoint.c buildsym.c c-lang.c \
	c-typeprint.c c-valprint.c ch-lang.c ch-typeprint.c \
	ch-valprint.c coffread.c command.c complaints.c copying.c \
	corelow.c cp-valprint.c \
	dcache.c dbxread.c demangle.c dwarfread.c \
	elfread.c environ.c eval.c exec.c expprint.c \
	findvar.c fork-child.c freebsd-nat.c gdbtypes.c \
	i386-tdep.c infcmd.c inflow.c infptrace.c \
	infrun.c inftarg.c init.c kcorelow.c language.c \
	m2-lang.c m2-typeprint.c m2-valprint.c main.c maint.c \
	mem-break.c minsyms.c objfiles.c parse.c \
	printcmd.c remote.c remote-utils.c solib.c source.c \
	stabsread.c stack.c symfile.c symmisc.c \
	symtab.c target.c thread.c top.c \
	typeprint.c utils.c valarith.c valops.c \
	valprint.c values.c version.c serial.c ser-unix.c mdebugread.c\
	c-exp.c f-exp.c m2-exp.c i387-tdep.c \
	kvm-fbsd.c bcache.c \
	corefile.c ch-exp.c f-lang.c scm-exp.c scm-lang.c \
	scm-valprint.c f-typeprint.c f-valprint.c nlmread.c \
	callback.c
SRCS+= i386-dis.c dis-buf.c disassemble.c

CFLAGS+= -I$(.CURDIR) -I${DESTDIR}/usr/include/readline -I$(.CURDIR)/../bfd
# use phkmalloc
CFLAGS+= -DNO_MMALLOC
# uncomment the next line if you want to debug gdb
#CFLAGS+= -g

CLEANFILES+=	c-exp.c f-exp.c m2-exp.c y.tab.h
beforedepend:	c-exp.c f-exp.c m2-exp.c

.if exists(${.OBJDIR}/../bfd)
LDADD+=   -L${.OBJDIR}/../bfd -lbfd
DPADD+=   ${.OBJDIR}/../bfd/libbfd.a
.else
LDADD+=   -L${.CURDIR}/../bfd/ -lbfd
DPADD+=   ${.CURDIR}/../bfd/libbfd.a
.endif

DPADD+= ${LIBREADLINE} ${LIBGNUREGEX}
LDADD+= -lreadline  -lgnuregex

.if exists(${.OBJDIR}/../libiberty)
LDADD+=   -L${.OBJDIR}/../libiberty -liberty
DPADD+=   ${.OBJDIR}/../libiberty/libiberty.a
.else
LDADD+=   -L${.CURDIR}/../libiberty/ -liberty
DPADD+=   ${.CURDIR}/../libiberty/libiberty.a
.endif

DPADD+= ${LIBTERMCAP}
LDADD+= -ltermcap

.if exists(${.OBJDIR}/../libiberty)
LDADD+=   -L${.OBJDIR}/../libiberty -liberty
DPADD+=   ${.OBJDIR}/../libiberty/libiberty.a
.else
LDADD+=   -L${.CURDIR}/../libiberty/ -liberty
DPADD+=   ${.CURDIR}/../libiberty/libiberty.a
.endif

.include <bsd.prog.mk>
