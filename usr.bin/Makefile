#	From: @(#)Makefile	8.3 (Berkeley) 1/7/94
#	$Id: Makefile,v 1.73.2.14 1998/04/18 10:56:31 obrien Exp $

# XXX MISSING:		deroff diction graph learn plot
#			spell spline struct units xsend
# XXX Use GNU versions: apropos bc dc diff grep ld man patch ptx uucp whatis
# Moved to secure: bdes
#
SUBDIR=	apply ar at banner basename biff brandelf c89 calendar \
	cap_mkdb chat checknr chflags chpass cksum col colcrt colldef colrm \
	column comm compile_et compress cpp ctags cut dig \
	dirname dnsquery du ee env error expand f2c false fetch file file2c \
	find finger fmt fold fpr from fsplit fstat ftp gcore gencat getopt \
	global gprof head hexdump host id indent ipcrm ipcs \
	join jot kdump ktrace key keyinfo keyinit killall lam last \
	lastcomm leave lex limits locate lock lockf logger login logname \
	lorder lsvfs m4 mail make mesg mkdep mkfifo mklocale mkstr mktemp mk_cmds \
	modstat more msgs mt ncal netstat nfsstat nice \
	nm nohup opieinfo opiekey opiepasswd \
	pagesize passwd paste pr printenv \
	printf quota ranlib rdist renice rev rlogin rpcgen \
	rpcinfo rs rsh rup ruptime rusers rwall \
	rwho script sed shar showmount size soelim split \
	strings	strip su symorder talk tconv tcopy tee tftp time \
	tip tn3270 top touch tput tr true tset tsort tty ul	uname \
	unexpand unifdef uniq units unvis users uudecode uuencode vacation \
	vgrind vi vis w wall wc what whereis which who whois window \
	write xargs xinstall xlint xstr yacc yes ypcat ypmatch ypwhich
SUBDIR+=gprof4 minigzip

.if !(exists(${.CURDIR}/../kerberosIV) && exists(${.CURDIR}/../crypto)) || defined(NOSECURE) || !defined(MAKE_KERBEROS4)
SUBDIR+=telnet
.else
.if defined(RELEASEDIR)
# releases do need both
SUBDIR+=telnet
.endif
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
