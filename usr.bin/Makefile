#	From: @(#)Makefile	8.3 (Berkeley) 1/7/94
#	$Id: Makefile,v 1.93 1997/12/15 20:52:24 helbig Exp $

# XXX MISSING:		deroff diction graph learn plot
#			spell spline struct units xsend
# XXX Use GNU versions: apropos bc dc diff grep ld man patch ptx uucp whatis
# Moved to secure: bdes
#
SUBDIR=	apply at banner basename biff c89 calendar \
	cap_mkdb chat checknr chflags chpass cksum col colcrt colldef colrm \
	column comm compile_et compress chkey cpp ctags cut dig \
	dirname dnsquery du ee env error expand \
	f2c false fetch file file2c \
	find finger fmt fold fpr from fsplit ftp gencat getopt \
	global head hexdump host id indent \
	join jot key keyinfo keyinit keylogin keylogout killall \
	lam last lastcomm leave lex limits locate lock lockf logger login \
	logname lsvfs m4 mail make mesg mkdep mkfifo mklocale mkstr \
	mk_cmds more msgs ncal netstat newkey nfsstat nice \
	nm nohup opieinfo opiekey opiepasswd pagesize passwd paste pr printenv \
	printf quota rdist renice rev rlogin rpcgen \
	rpcinfo rs rsh rup ruptime rusers rwall \
	rwho script sed shar showmount size soelim split \
	strings	su symorder talk tconv tcopy tee tftp time \
	tip tn3270 touch tput tr true tset tsort tty ul uname \
	unexpand unifdef uniq units unvis users uudecode uuencode vacation \
	vgrind vi vis wall wc what whereis which who whois window \
	write xargs xinstall xlint xstr yacc yes ypcat ypmatch ypwhich
SUBDIR+=minigzip

.if defined(RELEASEDIR) || !exists(${.CURDIR}/../kerberosIV) || defined(NOCRYPT) || !defined(MAKE_KERBEROS4)
# releases do need both
SUBDIR+=telnet
.endif

.if ${MACHINE_ARCH} == "i386"
# Things that don't compile on alpha:
SUBDIR+=ar brandelf doscmd fstat gcore gprof gprof4 ipcrm ipcs kdump ktrace \
	modstat mt ranlib strip top truss w lorder

.if !defined(NOTCL) && exists (${.CURDIR}/../contrib/tcl) && \
	exists(${.CURDIR}/tclsh) && exists (${.CURDIR}/../lib/libtcl)
SUBDIR+=tclsh
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
