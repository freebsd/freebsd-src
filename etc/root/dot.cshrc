alias mail Mail
set history=1000
set path=(/sbin /usr/sbin /bin /usr/bin /usr/local /usr/hosts /usr/contrib .)

# directory stuff: cdpath/cd/back
set cdpath=(/sys /usr/src/{bin,sbin,usr.{bin,sbin},pgrm,lib,libexec,share,contrib,local,devel,games,old,})
alias	cd	'set old=$cwd; chdir \!*'
alias	h	history
alias	j	jobs -l
alias	ll	ls -lg
alias	ls	ls -g -k
alias	back	'set back=$old; set old=$cwd; cd $back; unset back; dirs'

# sccs stuff: sd/co/ci/allout/out/unedit
alias	sd	sccs diffs
alias	co	sccs get -e
alias	ci	sccs delget
alias	allout	"(cd ..; echo */SCCS/p.*|sed s/SCCS\\/p.//g)"
alias	out	"echo SCCS/p.*|sed s/SCCS\\/p.//g"
alias	info	sccs info
alias	unedit	sccs unedit
alias	get	sccs get
alias	prt	sccs prt
alias	z		suspend
alias	x	exit
alias	pd	pushd
alias	pd2	pushd +2
alias	pd3	pushd +3
alias	pd4	pushd +4
alias	tset	'set noglob histchars=""; eval `\tset -s \!*`; unset noglob histchars'

if ($?prompt) then
	set prompt="`hostname -s`# "
endif
