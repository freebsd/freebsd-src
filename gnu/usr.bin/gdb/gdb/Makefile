PROG = gdb
BINDIR= /usr/bin
SRCS = main.c blockframe.c breakpoint.c findvar.c stack.c thread.c \
       source.c values.c eval.c valops.c valarith.c valprint.c printcmd.c \
       symtab.c symfile.c symmisc.c infcmd.c infrun.c command.c utils.c \
       expprint.c environ.c gdbtypes.c copying.c i386-tdep.c i386-pinsn.c \
       freebsd-solib.c ser-unix.c exec.c fork-child.c infptrace.c inftarg.c \
       corelow.c coredep.c freebsd-nat.c remote.c dcache.c remote-utils.c \
       mem-break.c target.c putenv.c parse.c language.c buildsym.c \
       objfiles.c minsyms.c maint.c demangle.c dbxread.c coffread.c \
       elfread.c dwarfread.c mipsread.c stabsread.c core.c c-lang.c \
       ch-lang.c m2-lang.c complaints.c typeprint.c c-typeprint.c \
       ch-typeprint.c m2-typeprint.c c-valprint.c cp-valprint.c ch-valprint.c \
       m2-valprint.c nlmread.c serial.c inflow.c regex.c init.c \
       c-exp.tab.c ch-exp.tab.c m2-exp.tab.c version.c i386-dis.c dis-buf.c

c-exp.tab.c: $(.CURDIR)/c-exp.y
	yacc -d -p c_ $(.CURDIR)/c-exp.y
	sed -e '/extern.*malloc/d'  -e '/extern.*realloc/d'  -e '/extern.*free/d' \
         -e '/include.*malloc.h/d'  -e 's/malloc/xmalloc/g'  \
         -e 's/realloc/xrealloc/g'  < y.tab.c > c-exp.new
	rm y.tab.c
	mv c-exp.new ./c-exp.tab.c

ch-exp.tab.c: $(.CURDIR)/ch-exp.y
	yacc -d -p ch_ $(.CURDIR)/ch-exp.y
	sed -e '/extern.*malloc/d'  -e '/extern.*realloc/d'  -e '/extern.*free/d' \
         -e '/include.*malloc.h/d'  -e 's/malloc/xmalloc/g'  \
         -e 's/realloc/xrealloc/g'  < y.tab.c > ch-exp.new
	rm y.tab.c
	mv ch-exp.new ./ch-exp.tab.c

m2-exp.tab.c: $(.CURDIR)/m2-exp.y
	yacc -d -p m2_ $(.CURDIR)/m2-exp.y
	sed -e '/extern.*malloc/d'  -e '/extern.*realloc/d'  -e '/extern.*free/d' \
         -e '/include.*malloc.h/d'  -e 's/malloc/xmalloc/g'  \
         -e 's/realloc/xrealloc/g'  < y.tab.c > m2-exp.new
	rm y.tab.c
	mv m2-exp.new ./m2-exp.tab.c



CFLAGS+= -I$(.CURDIR)/. -I/usr/include/readline -I$(.CURDIR)/../bfd
DPADD+= $(LIBREADLINE) $(LIBTERM)
LDADD+= -lreadline  -ltermcap

.if exists(${.CURDIR}/../libiberty/obj)
LDADD+=   -L${.CURDIR}/../libiberty/obj -liberty
DPADD+=   ${.CURDIR}/../libiberty/obj/libiberty.a
.else
LDADD+=   -L${.CURDIR}/../libiberty/ -liberty
DPADD+=   ${.CURDIR}/../libiberty/libiberty.a
.endif

.if exists(${.CURDIR}/../bfd/obj)
LDADD+=   -L${.CURDIR}/../bfd/obj -lbfd
DPADD+=   ${.CURDIR}/../bfd/obj/libbfd.a
.else
LDADD+=   -L${.CURDIR}/../bfd/ -lbfd
DPADD+=   ${.CURDIR}/../bfd/libbfd.a
.endif

.if exists(${.CURDIR}/../mmalloc/obj)
LDADD+=   -L${.CURDIR}/../mmalloc/obj -lmmalloc
DPADD+=   ${.CURDIR}/../mmalloc/obj/libmmalloc.a
.else
LDADD+=   -L${.CURDIR}/../mmalloc/ -lmmalloc
DPADD+=   ${.CURDIR}/../mmalloc/libmmalloc.a
.endif


.include <bsd.prog.mk>
