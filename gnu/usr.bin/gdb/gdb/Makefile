#	$FreeBSD$

PROG = gdb
BINDIR= /usr/bin
CLEANFILES+= y.tab.h c-exp.tab.c ch-exp.tab.c m2-exp.tab.c
SRCS = annotate.c blockframe.c breakpoint.c buildsym.c c-lang.c \
	c-typeprint.c c-valprint.c ch-lang.c ch-typeprint.c \
	ch-valprint.c coffread.c command.c complaints.c copying.c core.c \
	coredep.c corelow.c cp-valprint.c \
	dcache.c dbxread.c demangle.c disassemble.c dis-buf.c dwarfread.c \
	elfread.c environ.c eval.c exec.c expprint.c \
	findvar.c fork-child.c freebsd-nat.c gdbtypes.c i386-dis.c \
	i386-pinsn.c i386-tdep.c infcmd.c inflow.c infptrace.c \
	infrun.c inftarg.c init.c kcorelow.c language.c \
	m2-lang.c m2-typeprint.c m2-valprint.c main.c maint.c \
	mem-break.c minsyms.c objfiles.c parse.c \
	printcmd.c remote.c remote-utils.c solib.c source.c \
	stabsread.c stack.c symfile.c symmisc.c \
	symtab.c target.c thread.c top.c \
	typeprint.c utils.c valarith.c valops.c \
	valprint.c values.c version.c serial.c ser-unix.c mdebugread.c\
	c-exp.tab.c ch-exp.tab.c m2-exp.tab.c compat_que.c

c-exp.tab.c: $(.CURDIR)/c-exp.y
	yacc -d -p c_ $(.CURDIR)/c-exp.y
	sed -e '/extern.*malloc/d'  -e '/extern.*realloc/d'  -e '/extern.*free/d' \
         -e '/include.*malloc.h/d'  -e 's/malloc/xmalloc/g'  \
         -e 's/realloc/xrealloc/g'  < y.tab.c > c-exp.new
	rm -f y.tab.c
	mv -f c-exp.new ./c-exp.tab.c

ch-exp.tab.c: $(.CURDIR)/ch-exp.y
	yacc -d -p ch_ $(.CURDIR)/ch-exp.y
	sed -e '/extern.*malloc/d'  -e '/extern.*realloc/d'  -e '/extern.*free/d' \
         -e '/include.*malloc.h/d'  -e 's/malloc/xmalloc/g'  \
         -e 's/realloc/xrealloc/g'  < y.tab.c > ch-exp.new
	rm -f y.tab.c
	mv -f ch-exp.new ./ch-exp.tab.c

m2-exp.tab.c: $(.CURDIR)/m2-exp.y
	yacc -d -p m2_ $(.CURDIR)/m2-exp.y
	sed -e '/extern.*malloc/d'  -e '/extern.*realloc/d'  -e '/extern.*free/d' \
         -e '/include.*malloc.h/d'  -e 's/malloc/xmalloc/g'  \
         -e 's/realloc/xrealloc/g'  < y.tab.c > m2-exp.new
	rm -f y.tab.c
	mv -f m2-exp.new ./m2-exp.tab.c


CFLAGS+= -I$(.CURDIR)/. -I${DESTDIR}/usr/include/readline -I$(.CURDIR)/../bfd
DPADD+= ${LIBREADLINE} ${LIBTERMCAP} ${LIBGNUREGEX}
LDADD+= -lreadline  -ltermcap -lgnuregex

.if exists(${.OBJDIR}/../libiberty)
LDADD+=   -L${.OBJDIR}/../libiberty -liberty
DPADD+=   ${.OBJDIR}/../libiberty/libiberty.a
.else
LDADD+=   -L${.CURDIR}/../libiberty/ -liberty
DPADD+=   ${.CURDIR}/../libiberty/libiberty.a
.endif

.if exists(${.OBJDIR}/../bfd)
LDADD+=   -L${.OBJDIR}/../bfd -lbfd
DPADD+=   ${.OBJDIR}/../bfd/libbfd.a
.else
LDADD+=   -L${.CURDIR}/../bfd/ -lbfd
DPADD+=   ${.CURDIR}/../bfd/libbfd.a
.endif

.if exists(${.OBJDIR}/../mmalloc)
LDADD+=   -L${.OBJDIR}/../mmalloc -lmmalloc
DPADD+=   ${.OBJDIR}/../mmalloc/libmmalloc.a
.else
LDADD+=   -L${.CURDIR}/../mmalloc/ -lmmalloc
DPADD+=   ${.CURDIR}/../mmalloc/libmmalloc.a
.endif

.include <bsd.prog.mk>
