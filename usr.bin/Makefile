#	From: @(#)Makefile	8.3 (Berkeley) 1/7/94
#	$Id: Makefile,v 1.67 1996/03/24 14:09:33 paul Exp $

# XXX MISSING:		deroff diction graph learn plot
#			spell spline struct units xsend
# XXX Use GNU versions: apropos bc dc diff grep ld man patch ptx uucp whatis
# Moved to secure: bdes
#
SUBDIR=	apply ar at banner basename biff cal calendar \
	cap_mkdb chat checknr chflags chpass cksum col colcrt colldef colrm \
	column comm compile_et compress cpp ctags cut devmenu \
	dig dirname dnsquery du ee env error expand f2c false file file2c \
	find finger fmt fold fpr from fsplit fstat ftp gcore gencat getopt \
	gprof head hexdump host id indent ipcrm ipcs \
	join jot kdump ktrace key keyinfo keyinit killall lam last \
	lastcomm leave lex locate lock logger login logname lorder \
	lsvfs m4 mail make mesg mkdep mkfifo mklocale mkstr mk_cmds \
	modstat more msgs mt ncftp netstat nfsstat nice \
	nm nohup pagesize passwd paste pr printenv \
	printf quota ranlib rdist renice rev rlogin rpcgen \
	rpcinfo rs rsh rup ruptime rusers rwall \
	rwho script sed sgmlfmt sgmls shar showmount size soelim split \
	strings	strip su symorder talk tconv tcopy tee tftp time \
	tip tn3270 touch tput tr true tset tsort tty ul	uname \
	unexpand unifdef uniq units unvis users uudecode uuencode vacation \
	vgrind vi vis w wall wc what whereis which who whois window \
	write xargs xinstall xlint xstr yacc yes ypcat ypmatch ypwhich
SUBDIR+=gprof4

.if !exists(../secure) || defined(NOSECURE)
SUBDIR+=telnet
.else
.if defined(RELEASEDIR)
# releases do need both
SUBDIR+=telnet
.endif
SUBDIR+= ../secure/usr.bin/telnet
.endif

# Cmp, look and tail all use mmap, so new-VM only.
# F77 and pascal are VAX/Tahoe only.
.if	make(clean) || make(cleandir)
# XXX Should have `f77', `pascal' & `vmstat.sparc' judging by the
#     machine dependant lines, but we don't have them
SUBDIR+=cmp kzip look sasc systat tail vmstat
.elif	${MACHINE} == "hp300"
SUBDIR+=cmp ld look systat tail vmstat
.elif	${MACHINE} == "i386"
SUBDIR+=cmp kzip look sasc systat tail vmstat
# XXX Use gnu/usr.bin/ld for now
.elif	${MACHINE} == "luna68k"
SUBDIR+=cmp ld look systat tail vmstat
.elif	${MACHINE} == "mips"
SUBDIR+=cmp look systat tail vmstat
.elif	${MACHINE} == "sparc"
SUBDIR+=cmp ld look tail vmstat.sparc
.elif	${MACHINE} == "tahoe"
SUBDIR+=f77 pascal systat vmstat
.elif	${MACHINE} == "vax"
SUBDIR+=f77 pascal systat vmstat
.endif

.include <bsd.subdir.mk>
