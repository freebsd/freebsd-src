#	@(#)Makefile	8.3 (Berkeley) 1/7/94

SUBDIR=	apply apropos ar at banner basename bc bdes biff cal calendar \
	cap_mkdb checknr chflags chpass cksum col colcrt colrm column \
	comm compress cpp ctags cut dc deroff diction diff dirname du \
	env error ex expand false file find finger fmt fold fpr from \
	fsplit fstat ftp gcore gprof graph grep head hexdump id indent \
	join jot kdump ktrace lam last lastcomm learn leave locate \
	lock logger login logname lorder m4 mail make man mesg mkdep \
	mkfifo mklocale mkstr more msgs mt netstat nfsstat nice nm nohup \
	pagesize passwd paste patch plot pr printenv printf ptx quota \
	ranlib rdist renice rev rlogin rs rsh ruptime rwho sccs \
	script sed shar showmount size soelim sort spell spline split \
	strings strip struct su talk tcopy tee telnet tftp time tip \
	tn3270 touch tput tr true tset tsort tty ul uname unexpand \
	unifdef uniq units unvis users uucp uudecode uuencode vacation \
	vgrind vis w wall wc what whatis whereis who whois window write \
	xargs xinstall xsend xstr yacc yes

# Cmp, look and tail all use mmap, so new-VM only.
# F77 and pascal are VAX/Tahoe only.
.if	${MACHINE} == "hp300"
SUBDIR+=cmp ld look systat tail vmstat
.elif	${MACHINE} == "i386"
SUBDIR+=cmp ld look systat tail vmstat
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
