#	$Id: dot.cshrc,v 1.9 1994/02/21 20:35:58 rgrimes Exp $
#
alias mail Mail
set history=1000
set savehist=1000
set path=(/sbin /usr/sbin /bin /usr/bin /usr/local/bin)

# directory stuff: cdpath/cd/back
set cdpath=(/sys/{i386,} /usr/src/{bin,sbin,usr.{bin,sbin},lib,libexec,share,contrib,etc,games,gnu,include,})
alias	cd	'set old=$cwd; chdir \!*'
alias	h	history
alias	j	jobs -l
alias	ll	ls -lg
alias	ls	ls -g -k
alias	back	'set back=$old; set old=$cwd; cd $back; unset back; dirs'

alias	z	suspend
alias	x	exit
alias	pd	pushd
alias	pd2	pushd +2
alias	pd3	pushd +3
alias	pd4	pushd +4
alias	tset	'set noglob histchars=""; eval `\tset -s \!*`; unset noglob histchars'

if ($?prompt) then
	set prompt="`hostname -s`# "
	set filec
endif
setenv BLOCKSIZE K
