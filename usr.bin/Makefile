#	From: @(#)Makefile	8.3 (Berkeley) 1/7/94
#	$Id: Makefile,v 1.96 1998/03/22 08:55:28 jb Exp $

# XXX MISSING:		deroff diction graph learn plot
#			spell spline struct xsend
# XXX Use GNU versions: apropos bc dc diff grep ld man patch ptx uucp whatis
# Moved to secure: bdes
#
SUBDIR=	apply at banner basename biff c89 calendar \
	cap_mkdb chat checknr chflags chpass cksum cmp col colcrt colldef \
	colrm column comm compile_et compress chkey cpp ctags cut dig \
	dirname dnsquery du ee env error expand \
	f2c false fetch file file2c \
	find finger fmt fold fpr from fsplit ftp gencat getopt \
	global head hexdump host id indent \
	join jot key keyinfo keyinit keylogin keylogout killall kzip \
	lam last lastcomm leave lex limits locate lock lockf logger login \
	logname look lsvfs m4 mail make mesg mkdep mkfifo mklocale mkstr \
	mk_cmds more msgs ncal netstat newkey nfsstat nice \
	nohup opieinfo opiekey opiepasswd pagesize passwd paste pr printenv \
	printf quota rdist renice rev rlogin rpcgen \
	rpcinfo rs rsh rup ruptime rusers rwall \
	rwho script sed shar showmount size soelim split \
	strings	su symorder tail talk tconv tcopy tee tftp time \
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
	lorder modstat mt nm ranlib sasc strip systat top truss vmstat w

.if !defined(NOTCL) && exists (${.CURDIR}/../contrib/tcl) && \
	exists(${.CURDIR}/tclsh) && exists (${.CURDIR}/../lib/libtcl)
SUBDIR+=tclsh
.endif

.endif

.include <bsd.subdir.mk>
