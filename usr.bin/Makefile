#	@(#)Makefile	8.3 (Berkeley) 1/7/94

# XXX MISSING:		bc dc deroff diction diff ex graph learn plot
#			ptx spell spline struct units xsend
# XXX MISSING SOURCES:	file grep sort
# XXX MISSING Makefile:	uucp
# XXX Use GNU versions:	ld man apropos whatis patch
# Moved to secure: bdes
#
SUBDIR=	apply ar at banner basename biff cal calendar \
	cap_mkdb checknr chflags chpass cksum col colcrt colrm column \
	comm compress cpp ctags cut dirname du \
	env error expand false find finger fmt fold fpr from \
	fsplit fstat ftp gcore gprof head hexdump id indent \
	join jot kdump ktrace key keyinfo keyinit lam last \
	lastcomm leave lex locate lock logger login logname lorder \
	m4 mail make mesg mkdep mkfifo mklocale mkstr modstat \
	more msgs mt netstat nfsstat nice \
	nm nohup pagesize passwd paste pr printenv printf quota \
	ranlib rdist renice rev rlogin rs rsh ruptime rwho \
	script sed shar showmount size soelim split \
	strings strip su symorder talk tcopy tee telnet tftp time tip \
	tn3270 touch tput tr true tset tsort tty ul uname unexpand \
	unifdef uniq unvis users uudecode uuencode vacation \
	vgrind vi vis w wall wc what whereis who whois window write \
	xargs xinstall xstr yacc yes ypcat ypmatch ypwhich

# Cmp, look and tail all use mmap, so new-VM only.
# F77 and pascal are VAX/Tahoe only.
.if	${MACHINE} == "hp300"
SUBDIR+=cmp ld look systat tail vmstat
.elif	${MACHINE} == "i386"
SUBDIR+=cmp look systat tail vmstat
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
